"""
The validation harness: the black-box checks run against a live HARP gradio
app, independent of where that app is running.

Both tiers funnel into run_endpoint_tests(), which verifies that:

    1. the /controls and /process endpoints exist.
    2. /controls returns a well-formed model card and component spec.
    3. every configured /process test case produces valid outputs.

The drivers that get a model to that point live alongside: spaces.py for a
remote Hugging Face Space, examples.py for a local pyharp example.
"""

import argparse
import concurrent.futures
import time
from pathlib import Path

from gradio_client import Client

from assets import Assets, merge_specs
from cases import synthesize_default_args, apply_case, validate_outputs, inspect_outputs
from quota import is_zerogpu
from results import ModelResult, CaseResult, PASS, FAIL
from utils import run_with_timeout, describe_exception


__all__ = [
    'run_endpoint_tests',
    'call_with_retries',
    'close_client',
    'is_quota_exhausted'
]


# Fixed sub-timeouts and intervals, in seconds. Unlike connect_timeout /
# process_timeout, these bound quick calls whose duration does not vary by
# model, so they are module constants rather than per-model config.
CONTROLS_TIMEOUT = 120        # fetch the /controls spec (model already loaded)
CLIENT_CLOSE_TIMEOUT = 10     # best-effort gradio_client shutdown
CONNECT_RETRY_INTERVAL = 15   # wait between connection attempts to a model
LOADING_RETRY_INTERVAL = 15   # wait between retries while a model warms up
JOB_POLL_INTERVAL = 1         # cadence for polling a /process job's status

# gradio job status codes that mean the job is still waiting in the ZeroGPU
# queue (not yet executing on the GPU)
QUEUE_STATUS_CODES = {"IN_QUEUE", "JOINING_QUEUE", "QUEUE_FULL"}

# Substrings marking a "model is still loading" response. A ZeroGPU space
# waking its GPU worker returns this immediately rather than blocking, so it
# is retried (within connect_timeout) instead of treated as a failure.
LOADING_MARKERS = ("still loading", "loading, please wait", "is loading",
                   "currently loading", "warming up")

# The following are substrings marking an infrastructure fault rather than a
# fault in the model. These come from the connection or the GPU host. The same
# call typically succeeds on a retry, revealing the model's real behaviour.
# Retried a bounded number of times, since unlike a warm-up they have no
# expected duration to wait out.
#
# The list is deliberately narrow, holding only faults observed to clear on a
# retry. A retried /process call re-reserves ZeroGPU allowance, so a marker
# that also fires on a genuine, reproducible failure spends quota again and
# delays the real error by the retry interval. This rules out broad matches on
# a dropped connection (a bare "connection reset", or the RemoteProtocolError
# type name), because a model that crashes its own Space (an OOM, say) drops
# the connection in exactly the same way. The specific disconnect message below
# stays, since it was the one seen in practice. Extend this only with a fault
# confirmed to succeed on a retry.
TRANSIENT_MARKERS = ("read operation timed out",           # httpx ReadTimeout
                     "uncorrectable ecc error",            # GPU host fault
                     "server disconnected without sending a response")
TRANSIENT_RETRY_LIMIT = 3    # attempts after the first failure
TRANSIENT_RETRY_INTERVAL = 5  # seconds between them

# Substrings marking a ZeroGPU quota-exhausted response. Matching one skips
# every remaining ZeroGPU model, so these pair "quota" with an exhaustion verb
# rather than matching "gpu quota" anywhere: a model whose own error merely
# mentions a quota must not halt the rest of the run.
QUOTA_EXHAUSTED_MARKERS = ("exceeded your gpu quota", "gpu quota exceeded",
                           "quota exceeded")


def matches_markers(exc: Exception, markers: tuple) -> bool:
    """
    Whether an exception's type or message contains any of the markers.

    The searched text is "Type: message", mirroring how the error is written
    to the report, so a marker can be phrased against either part.

    Args:
        exc (Exception): The exception raised by a gradio call.
        markers (tuple): Lower-case substrings to look for.

    Returns:
        matched (bool): True if any marker is present.
    """

    text = f"{type(exc).__name__}: {exc}".lower()
    return any(marker in text for marker in markers)


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


def call_with_retries(fn, deadline: float):
    """
    Call fn(), retrying the failures that are not the model's fault.

    LOADING_MARKERS are retried until the deadline, the time budgeted for a
    model to come up. TRANSIENT_MARKERS are retried a bounded number of times
    instead. See those constants for what each covers and why.

    Args:
        fn (callable): Zero-argument call to make.
        deadline (float): time.time() value to stop retrying loading errors.

    Returns:
        result: Whatever fn() returns once a call succeeds.

    Raises:
        Exception: fn()'s error, once it is not retryable, the deadline has
            passed, or the transient retries are used up.
    """

    transient_attempts = 0

    while True:
        try:
            return fn()
        except Exception as exc:  # noqa: BLE001 - inspected below, else re-raised
            if matches_markers(exc, LOADING_MARKERS) and \
                    time.time() + LOADING_RETRY_INTERVAL < deadline:
                time.sleep(LOADING_RETRY_INTERVAL)
                continue
            if matches_markers(exc, TRANSIENT_MARKERS) and \
                    transient_attempts < TRANSIENT_RETRY_LIMIT:
                transient_attempts += 1
                time.sleep(TRANSIENT_RETRY_INTERVAL)
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

    A ZeroGPU job waits in a shared queue before running. That wait is not part
    of the model's execution and is not charged against quota, so it is bounded
    by queue_deadline (the connect timeout) rather than by the shorter
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
        client (Client | None): The client to close. None is a no-op.
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
    # Counted inside run_process so a job that reserves GPU is recorded even
    # when it then fails or times out, and a retried call counts again
    state = {"gpu_calls": 0}

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
            # The queue wait is bounded by connect_timeout. Only once the job
            # is dequeued does the (shorter) execution timeout apply
            wait_out_queue(job, time.time() + connect_timeout, "/process")
            state["gpu_calls"] += 1
            try:
                return job.result(timeout=process_timeout)
            except concurrent.futures.TimeoutError:
                # Cancelling ends the client's event stream for a job we have
                # stopped waiting on, so its worker thread does not linger for
                # the rest of the run. The upstream function itself keeps
                # running, as gradio can only call off a job still in the queue.
                job.cancel()
                # Future.result() raises with no message, so say what timed out
                raise TimeoutError(f"/process did not return within "
                                   f"{int(process_timeout)}s") from None

        output = call_with_retries(run_process, time.time() + connect_timeout)

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
        case_result.gpu_calls = state["gpu_calls"]

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

    # ZeroGPU models default to a lower process timeout, since their jobs are
    # short once running. A per-model process_timeout override still wins.
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
        result.error = (f"missing HARP endpoints (found: {sorted(named)}). "
                        f"The deployment may use an outdated pyharp")
        return result

    # --- /controls (retry through a warming-up model) ------------------------
    controls = call_with_retries(
        lambda: run_with_timeout(
            lambda: client.predict(api_name="/controls"), CONTROLS_TIMEOUT, "/controls"),
        time.time() + connect_timeout)
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
    # model's own cases. Their names are namespaced so both are legible in
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
