"""
Validation driver for local pyharp example apps.

Launches an example on a local port, waits for it to serve, and runs the
endpoint tests against it. The app's output is captured so a failure can
report the real traceback rather than the generic error the client sees.
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
from harness import run_endpoint_tests, close_client
from results import ModelResult
from utils import describe_exception


__all__ = [
    'test_local_example'
]


SERVER_PROBE_INTERVAL = 2       # poll cadence while a local example boots
SERVER_PROBE_TIMEOUT = 5        # per-probe HTTP timeout against a local example
PROCESS_TERMINATE_TIMEOUT = 15  # grace period before killing a local example


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

    The app runs in its own directory under <output-dir>/examples/, with
    stdout and stderr captured to app.log there. Keeping the app out of the
    example's source directory matters because pyharp writes model outputs to
    an `_outputs` folder under the working directory, which would otherwise
    accumulate inside the pyharp checkout.

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

    work_dir = opts.output_dir / "examples" / app_dir.name
    work_dir.mkdir(parents=True, exist_ok=True)
    log_path = work_dir / "app.log"
    proc = None
    client = None

    try:
        # Launched by absolute path so the app can run outside its own
        # directory. Python still puts that directory on sys.path, so an
        # example importing a module beside app.py keeps working.
        with open(log_path, "w") as log:
            proc = subprocess.Popen(
                [sys.executable, str((app_dir / "app.py").resolve())],
                cwd=work_dir, env=env, stdout=log, stderr=subprocess.STDOUT)
        wait_for_local_server(port, proc, overrides.get(
            "connect_timeout", opts.connect_timeout))
        client = Client(f"http://127.0.0.1:{port}", verbose=False)
        run_endpoint_tests(client, result, assets, overrides, opts)
        return result
    except Exception as exc:  # noqa: BLE001 - any failure means invalid
        result.error = (f"{describe_exception(exc)} "
                        f"(see examples/{work_dir.name}/{log_path.name})")
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
        # leaving it for the reader to locate in the log
        if result.error:
            detail = read_app_traceback(log_path)
            if detail and detail not in result.error:
                result.error = f"{result.error} | app log: {detail}"
