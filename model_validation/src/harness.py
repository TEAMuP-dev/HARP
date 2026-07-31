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
import contextlib
import os
import subprocess
import sys
import time
import traceback
import urllib.request
from pathlib import Path

from gradio_client import Client

from assets import Assets, merge_specs
from cases import synthesize_default_args, apply_case, validate_outputs, inspect_outputs
from quota import is_zerogpu
from results import ModelResult, CaseResult, PASS, FAIL, SKIP
from utils import run_with_timeout, scrub, describe_exception


__all__ = [
    'test_space',
    'test_local_example'
]


# Space runtime stages that indicate a hard failure (no point connecting)
DEAD_STAGES = {"BUILD_ERROR", "RUNTIME_ERROR", "CONFIG_ERROR", "DELETING"}
# Stages that resolve on their own if we wait (or wake on first request)
TRANSIENT_STAGES = {"BUILDING", "RUNNING_BUILDING", "APP_STARTING", "SLEEPING"}

# Fixed sub-timeouts, in seconds. Unlike connect_timeout / process_timeout,
# these bound quick metadata calls whose duration does not vary by model, so
# they are module constants rather than per-model config.
CONTROLS_TIMEOUT = 120       # fetch the /controls spec (model already loaded)
CLIENT_CLOSE_TIMEOUT = 10    # best-effort gradio_client shutdown
SERVER_PROBE_INTERVAL = 2    # poll cadence while a local example boots
SERVER_PROBE_TIMEOUT = 5     # per-probe HTTP timeout against a local example
PROCESS_TERMINATE_TIMEOUT = 15  # grace period before killing a local example
LOADING_RETRY_INTERVAL = 15  # wait between retries while a model warms up
JOB_POLL_INTERVAL = 1        # cadence for polling a /process job's status

# gradio job status codes that mean the job is still waiting in the ZeroGPU
# queue (not yet executing on the GPU)
QUEUE_STATUS_CODES = {"IN_QUEUE", "JOINING_QUEUE", "QUEUE_FULL"}

# Substrings marking a "model is still loading" response - a ZeroGPU space
# waking its GPU worker returns this immediately rather than blocking, so it
# is retried (within connect_timeout) instead of treated as a failure.
LOADING_MARKERS = ("still loading", "loading, please wait", "is loading",
                   "currently loading", "warming up")

# Substrings marking a ZeroGPU quota-exhausted response. Hitting this on one
# model means every other ZeroGPU model would hit it too, so the rest are
# skipped rather than retried.
QUOTA_EXHAUSTED_MARKERS = ("exceeded your gpu quota", "gpu quota exceeded",
                           "zerogpu quota", "quota exceeded", "gpu quota")


def is_loading_error(exc: Exception) -> bool:
    """
    Whether an exception is a transient "model still loading" response.

    Args:
        exc (Exception): The exception raised by a gradio call.

    Returns:
        loading (bool): True if the model reported it was still loading.
    """

    message = str(exc).lower()
    return any(marker in message for marker in LOADING_MARKERS)


def is_quota_exhausted(text: str) -> bool:
    """
    Whether an error message indicates the ZeroGPU allowance is exhausted.

    Args:
        text (str): An error message to inspect.

    Returns:
        exhausted (bool): True if it looks like a ZeroGPU quota error.
    """

    lowered = text.lower()
    return any(marker in lowered for marker in QUOTA_EXHAUSTED_MARKERS)


def call_through_loading(fn, deadline: float, what: str):
    """
    Call fn(), retrying while the model reports it is still loading.

    A ZeroGPU space that must start its GPU worker answers the first request
    with a "still loading" error immediately; retrying until the deadline
    tolerates that start-up interval instead of failing on it.

    Args:
        fn (callable): Zero-argument call to make.
        deadline (float): time.time() value to stop retrying at.
        what (str): Short description for the timeout message.

    Returns:
        result: Whatever fn() returns once the model is ready.

    Raises:
        Exception: fn()'s error, once it is not a loading error or the
            deadline has passed.
    """

    while True:
        try:
            return fn()
        except Exception as exc:  # noqa: BLE001 - inspected below, else re-raised
            if is_loading_error(exc) and time.time() + LOADING_RETRY_INTERVAL < deadline:
                time.sleep(LOADING_RETRY_INTERVAL)
                continue
            raise


def job_status_code(job) -> str | None:
    """
    Best-effort read of a gradio job's current status code name.

    Args:
        job: A gradio_client Job handle.

    Returns:
        code (str | None): The status code name (e.g. "IN_QUEUE",
            "PROCESSING"), or None if the status cannot be read.
    """

    try:
        status = job.status()
        return getattr(getattr(status, "code", None), "name", None)
    except Exception:  # noqa: BLE001 - status polling is best-effort
        return None


def wait_out_queue(job, queue_deadline: float, api_name: str) -> None:
    """
    Block until a /process job leaves the ZeroGPU queue and begins executing.

    A ZeroGPU job waits in a shared queue before running; that wait is not
    part of the model's execution and is not charged against quota, so it is
    bounded by queue_deadline (the connect timeout) rather than by the shorter
    execution timeout the caller applies afterwards.

    Args:
        job: A gradio_client Job handle.
        queue_deadline (float): time.time() by which the job must leave the
            queue, or it is cancelled.
        api_name (str): Endpoint name, for the timeout message.

    Raises:
        TimeoutError: If the job never leaves the queue by queue_deadline.
    """

    while not job.done() and job_status_code(job) in QUEUE_STATUS_CODES:
        if time.time() >= queue_deadline:
            job.cancel()
            raise TimeoutError(f"{api_name} still queued after waiting for the "
                               f"connect timeout")
        time.sleep(JOB_POLL_INTERVAL)


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
        run_with_timeout(client.close, CLIENT_CLOSE_TIMEOUT, "client close")
    except Exception:  # noqa: BLE001 - cleanup is best-effort
        pass


def run_case(client: Client, case: dict, controls: dict, assets: Assets,
             overrides: dict, config_dir: Path, process_timeout: float,
             connect_timeout: float) -> CaseResult:
    """
    Run one /process test case and check its outputs.

    The whole case sits inside an error boundary: any failure is recorded on
    the returned record rather than raised, so remaining cases still run.

    Args:
        client (Client): Connected gradio client for the deployment.
        case (dict): Test case entry from config.yml.
        controls (dict): The /controls payload.
        assets (Assets): Synthesized input files.
        overrides (dict): This model's entry from config.yml `overrides`.
        config_dir (Path): Directory containing config.yml.
        process_timeout (float): Seconds allowed for /process execution.
        connect_timeout (float): Seconds allowed for the queue wait and for
            retrying a model that is still loading.

    Returns:
        case_result (CaseResult): The completed case record.
    """

    case_result = CaseResult(name=case.get("name", "unnamed"))
    start = time.time()
    # Set inside run_process so `executed` is recorded even when the job leaves
    # the queue (and so reserves GPU) but then fails or times out
    state = {"executed": False}

    try:
        synth = merge_specs(overrides.get("synthesized_inputs"),
                            case.get("synthesized_inputs"))
        default_args, missing = synthesize_default_args(controls, assets, synth)

        # Inputs we could not synthesize are fine if this case supplies them
        supplied = set(case.get("controls") or {}) | set(case.get("files") or {})
        unsatisfied = {k: v for k, v in missing.items() if k not in supplied}
        if unsatisfied:
            case_result.error = "skipped: " + "; ".join(
                f"'{k}': {v}" for k, v in unsatisfied.items())
            return case_result

        args = apply_case(default_args, controls, case, config_dir)

        def run_process():
            job = client.submit(*args, api_name="/process")
            # The queue wait is bounded by connect_timeout; only once the job
            # is dequeued does the (shorter) execution timeout apply
            wait_out_queue(job, time.time() + connect_timeout, "/process")
            state["executed"] = True
            return job.result(timeout=process_timeout)

        output = call_through_loading(
            run_process, time.time() + connect_timeout, "/process")

        error = validate_outputs(output, controls)
        if error:
            case_result.ok = False
            case_result.error = f"/process output invalid: {error}"
        else:
            inspect_outputs(output, controls, case)
            case_result.ok = True
    except Exception as exc:  # noqa: BLE001 - any exception fails the case
        case_result.ok = False
        case_result.error = describe_exception(exc)
    finally:
        case_result.duration = round(time.time() - start, 1)
        case_result.executed = state["executed"]

    return case_result


def run_endpoint_tests(client: Client, result: ModelResult, assets: Assets,
                       overrides: dict, opts: argparse.Namespace) -> ModelResult:
    """
    Verify /controls and run every configured /process test case.

    Each case runs inside its own error boundary, so a failing case does not
    stop the remaining cases.

    Args:
        client (Client): Connected gradio client for the deployment.
        result (ModelResult): Result record to fill in.
        assets (Assets): Synthesized input files.
        overrides (dict): This model's entry from config.yml `overrides`.
        opts (argparse.Namespace): Parsed command-line options.

    Returns:
        result (ModelResult): The same record, completed.
    """

    # ZeroGPU models default to a lower process timeout (their jobs are short
    # once running); a per-model process_timeout override still wins
    default_process_timeout = (opts.zerogpu_process_timeout
                               if is_zerogpu(result.hardware)
                               else opts.process_timeout)
    process_timeout = overrides.get("process_timeout", default_process_timeout)
    connect_timeout = overrides.get("connect_timeout", opts.connect_timeout)
    load_only = opts.load_only or overrides.get("load_only", False)
    config_dir = opts.config.parent.resolve()

    # --- Endpoint presence ---------------------------------------------------
    endpoints = client.view_api(return_format="dict", print_info=False) or {}
    named = endpoints.get("named_endpoints", {})
    if "/controls" not in named or "/process" not in named:
        result.error = (f"missing HARP endpoints (found: {sorted(named)}); "
                        f"deployment may use an outdated pyharp")
        return result

    # --- /controls (retry through a warming-up model) ------------------------
    controls = call_through_loading(
        lambda: run_with_timeout(
            lambda: client.predict(api_name="/controls"), CONTROLS_TIMEOUT, "/controls"),
        time.time() + connect_timeout, "/controls")
    if not isinstance(controls, dict) or "card" not in controls or "inputs" not in controls:
        result.error = f"/controls returned malformed data: {str(controls)[:200]}"
        return result
    result.controls_ok = True
    result.model_name = controls.get("card", {}).get("name", "")

    if load_only:
        result.status = PASS
        return result

    # --- /process test cases -------------------------------------------------
    # Common cases (generic, applied to every model) run alongside this
    # model's own cases; their names are namespaced so both are legible in
    # the report. A model can opt out with `skip_common_cases`.
    own_cases = overrides.get("test_cases") or [{"name": "default"}]
    if overrides.get("skip_common_cases"):
        common_cases = []
    else:
        common_cases = [dict(c, name=f"common:{c.get('name', 'unnamed')}")
                        for c in (opts.common_test_cases or [])]
    cases = common_cases + own_cases

    for case in cases:
        result.cases.append(run_case(
            client, case, controls, assets, overrides, config_dir,
            case.get("process_timeout", process_timeout), connect_timeout))

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
               opts: argparse.Namespace, overrides: dict,
               quota_exhausted=None, zerogpu_limiter=None) -> ModelResult:
    """
    Validate one remote Hugging Face Space end-to-end.

    Args:
        space_id (str): The space to validate (e.g. "teamup-tech/foo").
        token (str): Hugging Face access token.
        assets (Assets): Synthesized input files.
        opts (argparse.Namespace): Parsed command-line options.
        overrides (dict): This space's entry from config.yml `overrides`.
        quota_exhausted (threading.Event | None): Shared flag; when set,
            ZeroGPU models are skipped, and this model sets it if its own run
            reveals the allowance is exhausted.
        zerogpu_limiter (threading.Semaphore | None): Caps how many ZeroGPU
            models make GPU calls at once. Held only around the endpoint
            tests, so slow restarts and connections still overlap.

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
        zerogpu = is_zerogpu(result.hardware)
        # Hardware is occasionally unreported for a sleeping or starting space.
        # Throttle those as if they were ZeroGPU: being wrong only costs run
        # time, whereas leaving a real ZeroGPU model unthrottled overlaps GPU
        # reservations. Skipping, by contrast, stays strict - wrongly skipping
        # a CPU model would silently drop it from validation.
        throttle = zerogpu or not result.hardware

        # Skip ZeroGPU models before doing any work that would spend quota
        if zerogpu and opts.skip_zerogpu:
            result.status = SKIP
            result.error = "skipped: ZeroGPU hardware (--skip-zerogpu)"
            return result
        if zerogpu and quota_exhausted is not None and quota_exhausted.is_set():
            result.status = SKIP
            result.error = "skipped: ZeroGPU allowance already exhausted this run"
            return result

        if stage in DEAD_STAGES or stage in ("STOPPED", "PAUSED"):
            if opts.restart_failed and stage != "DELETING":
                print(f"  [{space_id}] stage={stage}, requesting restart...")
                try:
                    api.restart_space(space_id)
                except Exception as exc:  # noqa: BLE001 - e.g. read-only token
                    result.error = scrub(
                        f"space is not running (stage={stage}) and restart "
                        f"failed: {describe_exception(exc)}", token)
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
        # Connecting has woken the space; reflect that rather than the stale
        # SLEEPING/STARTING stage seen before the wake-up, and fill in the
        # hardware if it was not reported while the space was asleep
        if result.stage != "RUNNING":
            result.stage = "RUNNING"
        if not result.hardware:
            try:
                awake = api.get_space_runtime(space_id)
                result.hardware = awake.hardware or awake.requested_hardware or ""
                zerogpu = is_zerogpu(result.hardware)
            except Exception:  # noqa: BLE001 - hardware stays unknown
                pass

        # Serialize ZeroGPU models: concurrent GPU calls each reserve their
        # declared duration, so overlapping them ties up allowance that is not
        # actually being used
        limiter = zerogpu_limiter if (throttle and zerogpu_limiter is not None) \
            else contextlib.nullcontext()
        with limiter:
            # The allowance may have run out while waiting for a slot
            if zerogpu and quota_exhausted is not None and quota_exhausted.is_set():
                result.status = SKIP
                result.error = "skipped: ZeroGPU allowance already exhausted this run"
                return result

            run_endpoint_tests(client, result, assets, overrides, opts)

        # If a ZeroGPU model failed because the allowance is gone, trip the
        # guard so the remaining ZeroGPU models are skipped
        if zerogpu and quota_exhausted is not None and result.status == FAIL \
                and is_quota_exhausted(result.error):
            quota_exhausted.set()

        return result

    except Exception as exc:  # noqa: BLE001 - any failure means invalid
        result.error = scrub(describe_exception(exc), token)
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
            with urllib.request.urlopen(url, timeout=SERVER_PROBE_TIMEOUT):
                return
        except Exception:  # noqa: BLE001 - server not up yet
            time.sleep(SERVER_PROBE_INTERVAL)

    raise TimeoutError(f"local app did not become ready within {int(timeout)}s")


def read_app_traceback(log_path: Path, max_chars: int = 400) -> str:
    """
    Extract the final exception line from a local app's captured log.

    Args:
        log_path (Path): The app's stdout/stderr log.
        max_chars (int): Cap on the returned text.

    Returns:
        detail (str): The last traceback's exception line, or "" if the log
            holds no traceback (or cannot be read).
    """

    try:
        lines = log_path.read_text(errors="replace").splitlines()
    except OSError:
        return ""

    # The exception line is the last non-indented line after the last "Traceback"
    starts = [i for i, line in enumerate(lines) if line.startswith("Traceback")]
    if not starts:
        return ""
    tail = [line for line in lines[starts[-1] + 1:]
            if line.strip() and not line.startswith((" ", "\t"))]

    return tail[-1].strip()[:max_chars] if tail else ""


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
        run_endpoint_tests(client, result, assets, overrides, opts)
        return result
    except Exception as exc:  # noqa: BLE001 - any failure means invalid
        result.error = f"{describe_exception(exc)} (see {log_path.name})"
        if opts.verbose:
            traceback.print_exc()
        return result
    finally:
        result.duration = round(time.time() - start, 1)
        close_client(client)
        if proc is not None and proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=PROCESS_TERMINATE_TIMEOUT)
            except subprocess.TimeoutExpired:
                proc.kill()
        # A local app logs the real traceback even when the client is only
        # told "an error occurred", so fold it into the report rather than
        # leaving it in a file the reader has to go find
        if result.error:
            detail = read_app_traceback(log_path)
            if detail and detail not in result.error:
                result.error = f"{result.error} | app log: {detail}"
