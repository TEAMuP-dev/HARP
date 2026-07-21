"""
The core validation harness for HARP model deployments: the code that sets
up a connection to a live model, drives it through /controls and /process,
and reports what happened, independent of where the model is running.

Both validation tiers funnel into run_endpoint_tests(), which performs the
identical black-box checks against any live HARP gradio app:

    1. the /controls and /process endpoints exist;
    2. /controls returns a well-formed model card and component spec;
    3. every configured /process test case produces valid outputs.

test_space() wraps this for a remote Hugging Face Space (runtime stage
checks, optional restart, connection with wake-up retries) and
test_local_example() for a pyharp example app launched locally.
"""

import argparse
import os
import subprocess
import sys
import time
import traceback
import urllib.request
from pathlib import Path

from gradio_client import Client

from assets import Assets
from cases import synthesize_default_args, apply_case, validate_outputs, inspect_outputs
from results import ModelResult, CaseResult, PASS, FAIL
from utils import run_with_timeout, scrub


__all__ = [
    'test_space',
    'test_local_example'
]


# Space runtime stages that indicate a hard failure (no point connecting)
DEAD_STAGES = {"BUILD_ERROR", "RUNTIME_ERROR", "CONFIG_ERROR", "DELETING"}
# Stages that resolve on their own if we wait (or wake on first request)
TRANSIENT_STAGES = {"BUILDING", "RUNNING_BUILDING", "APP_STARTING", "SLEEPING"}


def close_client(client) -> None:
    """
    Shut down a gradio_client instance without letting cleanup errors
    (or hangs in its heartbeat machinery) affect the result.

    Args:
        client (Client | None): The client to close; None is a no-op.
    """

    if client is None:
        return

    try:
        run_with_timeout(client.close, 10, "client close")
    except Exception:  # noqa: BLE001 - cleanup is best-effort
        pass


def run_endpoint_tests(client: Client, result: ModelResult, assets: Assets,
                       overrides: dict, opts: argparse.Namespace) -> ModelResult:
    """
    Verify /controls and run every configured /process test case.

    Each case runs inside its own error boundary, so a failing case never
    stops the remaining cases (or models) from running.

    Args:
        client (Client): Connected gradio client for the deployment.
        result (ModelResult): Result record to fill in.
        assets (Assets): Synthesized input files.
        overrides (dict): This model's entry from config.yml `overrides`.
        opts (argparse.Namespace): Parsed command-line options.

    Returns:
        result (ModelResult): The same record, completed.
    """

    process_timeout = overrides.get("process_timeout", opts.process_timeout)
    load_only = opts.load_only or overrides.get("load_only", False)
    config_dir = opts.config.parent.resolve()

    # --- Endpoint presence ---------------------------------------------------
    endpoints = client.view_api(return_format="dict", print_info=False) or {}
    named = endpoints.get("named_endpoints", {})
    if "/controls" not in named or "/process" not in named:
        result.error = (f"missing HARP endpoints (found: {sorted(named)}); "
                        f"deployment may use an outdated pyharp")
        return result

    # --- /controls -----------------------------------------------------------
    controls = run_with_timeout(
        lambda: client.predict(api_name="/controls"), 120, "/controls")
    if not isinstance(controls, dict) or "card" not in controls or "inputs" not in controls:
        result.error = f"/controls returned malformed data: {str(controls)[:200]}"
        return result
    result.controls_ok = True
    result.model_name = controls.get("card", {}).get("name", "")

    if load_only:
        result.status = PASS
        return result

    # --- /process test cases -------------------------------------------------
    default_args, missing = synthesize_default_args(controls, assets)
    cases = overrides.get("test_cases") or [{"name": "default"}]

    for case in cases:
        case_result = CaseResult(name=case.get("name", "unnamed"))
        result.cases.append(case_result)
        case_start = time.time()
        try:
            # Inputs we couldn't synthesize are fine if this case supplies them
            supplied = set(case.get("controls") or {}) | set(case.get("files") or {})
            unsatisfied = {k: v for k, v in missing.items() if k not in supplied}
            if unsatisfied:
                case_result.error = "skipped: " + "; ".join(
                    f"'{k}': {v}" for k, v in unsatisfied.items())
                continue

            args = apply_case(default_args, controls, case, config_dir)
            job = client.submit(*args, api_name="/process")
            output = job.result(timeout=case.get("process_timeout", process_timeout))

            error = validate_outputs(output, controls)
            if error:
                case_result.ok = False
                case_result.error = f"/process output invalid: {error}"
            else:
                inspect_outputs(output, controls, case)
                case_result.ok = True
        except Exception as exc:  # noqa: BLE001 - any exception fails the case
            case_result.ok = False
            case_result.error = f"{type(exc).__name__}: {exc}"
        finally:
            case_result.duration = round(time.time() - case_start, 1)

    if any(c.ok is False for c in result.cases):
        result.error = "; ".join(
            f"[{c.name}] {c.error}" for c in result.cases if c.ok is False)
        result.status = FAIL
    else:
        result.status = PASS

    return result


# ---------------------------------------------------------------------------
# Remote Hugging Face Spaces
# ---------------------------------------------------------------------------

def wait_for_runtime(api, space_id: str, deadline: float):
    """
    Poll a space's runtime until its stage settles or the deadline passes.

    Args:
        api (HfApi): Authenticated Hugging Face API client.
        space_id (str): The space to poll.
        deadline (float): time.time() value to stop polling at.

    Returns:
        runtime (SpaceRuntime): The last observed runtime (stage, hardware).
    """

    runtime = api.get_space_runtime(space_id)

    # SLEEPING resolves on first request rather than by waiting
    while runtime.stage in TRANSIENT_STAGES - {"SLEEPING"} and time.time() < deadline:
        time.sleep(10)
        runtime = api.get_space_runtime(space_id)

    return runtime


def test_space(space_id: str, token: str, assets: Assets,
               opts: argparse.Namespace, overrides: dict) -> ModelResult:
    """
    Validate one remote Hugging Face Space end-to-end.

    Args:
        space_id (str): The space to validate (e.g. "teamup-tech/foo").
        token (str): Hugging Face access token.
        assets (Assets): Synthesized input files.
        opts (argparse.Namespace): Parsed command-line options.
        overrides (dict): This space's entry from config.yml `overrides`.

    Returns:
        result (ModelResult): The completed validation record.
    """

    # Imported lazily so the examples tier works without huggingface_hub
    from huggingface_hub import HfApi

    result = ModelResult(target=space_id, kind="space")
    start = time.time()
    api = HfApi(token=token)
    connect_timeout = overrides.get("connect_timeout", opts.connect_timeout)
    client = None

    try:
        # --- Runtime stage (restart crashed/stopped spaces by default) -------
        deadline = time.time() + connect_timeout
        runtime = wait_for_runtime(api, space_id, deadline)
        stage = runtime.stage
        result.stage = stage
        # requested_hardware reflects the space's configuration even while
        # it is sleeping or stopped (hardware itself is only set when live)
        result.hardware = runtime.requested_hardware or runtime.hardware or ""

        if stage in DEAD_STAGES or stage in ("STOPPED", "PAUSED"):
            if opts.restart_failed and stage != "DELETING":
                print(f"  [{space_id}] stage={stage}, requesting restart...")
                try:
                    api.restart_space(space_id)
                except Exception as exc:  # noqa: BLE001 - e.g. read-only token
                    result.error = scrub(
                        f"space is not running (stage={stage}) and restart "
                        f"failed: {type(exc).__name__}: {exc}", token)
                    return result
                runtime = wait_for_runtime(api, space_id, time.time() + connect_timeout)
                stage = runtime.stage
                result.stage = stage
            if stage not in ("RUNNING", "SLEEPING"):
                result.error = f"space is not running (stage={stage})"
                return result

        # --- Connect (wakes sleeping spaces; retry through the wake-up) ------
        last_exc = None
        deadline = time.time() + connect_timeout
        while time.time() < deadline:
            try:
                client = run_with_timeout(
                    lambda: Client(space_id, hf_token=token, verbose=False),
                    max(10.0, deadline - time.time()), "connect")
                break
            except Exception as exc:  # noqa: BLE001 - space may still be waking
                last_exc = exc
                time.sleep(15)
        if client is None:
            raise RuntimeError(f"could not connect: {last_exc}")

        return run_endpoint_tests(client, result, assets, overrides, opts)

    except Exception as exc:  # noqa: BLE001 - any failure means invalid
        result.error = scrub(f"{type(exc).__name__}: {exc}", token)
        if opts.verbose:
            traceback.print_exc()
        return result
    finally:
        result.duration = round(time.time() - start, 1)
        close_client(client)


# ---------------------------------------------------------------------------
# Local pyharp examples
# ---------------------------------------------------------------------------

def wait_for_local_server(port: int, proc: subprocess.Popen, timeout: float) -> None:
    """
    Block until a locally launched gradio app starts serving.

    Args:
        port (int): The port the app was told to bind (GRADIO_SERVER_PORT).
        proc (subprocess.Popen): The app process, watched for early exit.
        timeout (float): Seconds to wait before giving up.

    Raises:
        RuntimeError: If the app process exits before serving.
        TimeoutError: If the app is not serving within the timeout.
    """

    deadline = time.time() + timeout
    url = f"http://127.0.0.1:{port}/config"

    while time.time() < deadline:
        if proc.poll() is not None:
            raise RuntimeError(f"app exited early with code {proc.returncode}")
        try:
            with urllib.request.urlopen(url, timeout=5):
                return
        except Exception:  # noqa: BLE001 - server not up yet
            time.sleep(2)

    raise TimeoutError(f"local app did not become ready within {int(timeout)}s")


def test_local_example(app_dir: Path, port: int, assets: Assets,
                       opts: argparse.Namespace, overrides: dict) -> ModelResult:
    """
    Validate one pyharp example app by launching it locally.

    The app's stdout/stderr are captured to <output-dir>/<example>.log for
    debugging failures.

    Args:
        app_dir (Path): Example directory containing app.py.
        port (int): Local port to launch the app on.
        assets (Assets): Synthesized input files.
        opts (argparse.Namespace): Parsed command-line options.
        overrides (dict): This example's entry from config.yml `overrides`
            (keyed "examples/<example-dir>").

    Returns:
        result (ModelResult): The completed validation record.
    """

    target = f"examples/{app_dir.name}"
    result = ModelResult(target=target, kind="local", stage="LOCAL")
    start = time.time()

    env = os.environ.copy()
    env["GRADIO_SERVER_NAME"] = "127.0.0.1"
    env["GRADIO_SERVER_PORT"] = str(port)
    env.pop("HF_TOKEN", None)  # local examples must not need credentials

    log_path = opts.output_dir / f"{app_dir.name}.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)
    proc = None
    client = None

    try:
        with open(log_path, "w") as log:
            proc = subprocess.Popen(
                [sys.executable, "app.py"], cwd=app_dir, env=env,
                stdout=log, stderr=subprocess.STDOUT)
        wait_for_local_server(port, proc, overrides.get(
            "connect_timeout", opts.connect_timeout))
        client = Client(f"http://127.0.0.1:{port}", verbose=False)
        return run_endpoint_tests(client, result, assets, overrides, opts)
    except Exception as exc:  # noqa: BLE001 - any failure means invalid
        result.error = f"{type(exc).__name__}: {exc} (see {log_path.name})"
        if opts.verbose:
            traceback.print_exc()
        return result
    finally:
        result.duration = round(time.time() - start, 1)
        close_client(client)
        if proc is not None and proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=15)
            except subprocess.TimeoutExpired:
                proc.kill()
