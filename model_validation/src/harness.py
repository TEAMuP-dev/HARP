"""
The validation harness: the black-box checks run against a live HARP gradio
app, independent of where that app is running.

Both tiers funnel into run_endpoint_tests(), which verifies that:

    1. the /controls and /process endpoints exist.
    2. /controls returns a well-formed model card and component spec, and,
       for a Space, one whose tags and name its README repeats.
    3. every configured /process test case produces valid outputs, or for a
       case expecting an error, refuses its input with a gr.Error.

The drivers that get a model to that point live alongside: spaces.py for a
remote Hugging Face Space, examples.py for a local pyharp example.
"""

import argparse
import concurrent.futures
import time
from pathlib import Path

from gradio_client import Client

from assets import Assets, input_kind, merge_specs
from cases import (synthesize_default_args, apply_case, validate_outputs,
                   inspect_outputs, measure_outputs, fed_kinds)
from refusals import install as install_refusals, judge_refusal
from results import ModelResult, CaseResult, PASS, FAIL, SKIP
from utils import run_with_timeout, describe_exception


# Lets a case expecting an error tell a gr.Error from any other exception
install_refusals()


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
JOB_POLL_INTERVAL = 1         # cadence for polling the status of a /process job

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
# call typically succeeds on a retry, revealing the real behavior of the model.
# Retried a bounded number of times, since unlike a warm-up they have no
# expected duration.
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
    Whether the type or message of an exception contains any of the markers.

    The searched text is "Type: message", mirroring how the error is written
    to the report, so a marker can be phrased against either part.

    Args:
        exc (Exception): The exception raised by a gradio call.
        markers (tuple): Lower-case substrings to find.

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
    Call fn(), retrying the failures that are not the fault of the model.

    LOADING_MARKERS are retried until the deadline, the time budgeted for a
    model to start. TRANSIENT_MARKERS are retried a bounded number of times
    instead. See those constants for what each covers and why.

    Args:
        fn (callable): Zero-argument call to make.
        deadline (float): time.time() value to stop retrying loading errors.

    Returns:
        result: Whatever fn() returns once a call succeeds.

    Raises:
        Exception: fn()'s error, once it is not retryable, the deadline has
            passed, or the transient retries are spent.
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
    Best-effort read of the current status code name of a gradio job.

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
    of the execution of the model and is not charged against quota, so it is bounded
    by queue_deadline (the connect timeout) rather than by the shorter
    execution timeout the caller applies afterwards.

    Args:
        job: A gradio_client Job handle.
        queue_deadline (float): time.time() by which the job must leave the
            queue, or it is canceled.
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


def corrupted_kinds(assets: Assets, synth: dict) -> set:
    """
    The media kinds for which a case sends an input to refuse.

    Args:
        assets (Assets): Synthesized input files, which hold the defaults.
        synth (dict): The merged `synthesized_inputs` block of the case.

    Returns:
        kinds (set): Any of "audio" and "midi" whose `content` is set.
    """

    return {kind for kind in ("audio", "midi") if assets.resolve(kind, synth).get("content")}


def run_case(client: Client, case: dict, controls: dict, assets: Assets,
             overrides: dict, config_dir: Path, process_timeout: float,
             connect_timeout: float) -> CaseResult:
    """
    Run one /process test case and check its outputs.

    A case with `expect_error` passes instead when /process refuses its input
    with a gr.Error (see refusals.py), and fails when it returns outputs.

    The whole case sits inside an error boundary: any failure is recorded on
    the returned record rather than raised, so remaining cases still run.

    Args:
        client (Client): Connected gradio client for the deployment.
        case (dict): Test case entry from config.yml.
        controls (dict): The /controls payload.
        assets (Assets): Synthesized input files.
        overrides (dict): The entry of this model in config.yml `overrides`.
        config_dir (Path): Directory containing config.yml.
        process_timeout (float): Seconds allowed for /process execution. The
            setting of the model, which the `process_timeout` of a case overrides.
        connect_timeout (float): Seconds allowed for the queue wait and for
            retrying a model that is still loading.

    Returns:
        case_result (CaseResult): The completed case record.
    """

    case_result = CaseResult(name=case.get("name", "unnamed"))
    start = time.time()
    process_timeout = case.get("process_timeout", process_timeout)
    expected_error = case.get("expect_error")
    # Counted inside run_process so a job that reserves GPU is recorded even
    # when it then fails or times out, and a retried call counts again
    state = {"gpu_calls": 0}

    try:
        synth = merge_specs(overrides.get("synthesized_inputs"),
                            case.get("synthesized_inputs"))
        default_args, missing = synthesize_default_args(controls, assets, synth)

        # An input to refuse replaces every input of its kind, so the files a
        # common case inherits from the cases of the model must not restore them
        corrupted = corrupted_kinds(assets, synth)
        if corrupted:
            kinds = {spec.get("label"): input_kind(spec) for spec in controls.get("inputs", [])}
            case = dict(case, files={label: path for label, path
                                     in (case.get("files") or {}).items()
                                     if kinds.get(label) not in corrupted})

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
                # Canceling ends the event stream of the client for a job we have
                # stopped waiting on, so its worker thread does not linger for
                # the rest of the run. The upstream function itself keeps
                # running, as gradio can only call off a job still in the queue.
                job.cancel()
                # Future.result() raises with no message, so say what timed out
                raise TimeoutError(f"/process did not return within "
                                   f"{int(process_timeout)}s") from None

        if expected_error:
            try:
                call_with_retries(run_process, time.time() + connect_timeout)
            except Exception as exc:  # noqa: BLE001 - judged as the expected refusal
                case_result.ok, case_result.error, reached_gpu = \
                    judge_refusal(exc, expected_error)
                # A refusal raised by the checks of the model reserved nothing
                if not reached_gpu:
                    state["gpu_calls"] = 0
            else:
                case_result.ok = False
                case_result.error = "returned outputs, where it should have refused the input"
            return case_result

        output = call_with_retries(run_process, time.time() + connect_timeout)

        error = validate_outputs(output, controls)
        if error:
            case_result.ok = False
            case_result.error = f"/process output invalid: {error}"
        else:
            case_result.observed = measure_outputs(output, controls)
            inspect_outputs(output, controls, case)
            case_result.ok = True
    except Exception as exc:  # noqa: BLE001 - any exception fails the case
        case_result.ok = False
        case_result.error = describe_exception(exc)
    finally:
        case_result.duration = round(time.time() - start, 1)
        case_result.gpu_calls = state["gpu_calls"]

    return case_result


# A common case that sends nothing of its own is a set of checks rather than a
# scenario, so what it asserts is folded into the cases the model already runs.
INPUT_KEYS = ("controls", "files", "synthesized_inputs")


def merge_common_cases(common_cases: list, own_cases: list) -> list:
    """
    Combine the common cases with those of a model, sharing /process calls.

    Every case costs one /process call, and on ZeroGPU each call reserves
    allowance from a daily budget that a full run of the fleet exhausts. A
    common case that declares no inputs of its own asks the same question of
    the same outputs as the cases of the model, so running it
    separately buys no coverage and triples the cost: its `expect` rules and
    `validators` are merged into each own case instead.

    A common case that does declare inputs is a scenario in its own right and
    still runs separately, with its name namespaced for the report. It inherits
    whatever the first case of the model sends and does not override, so a model
    that needs particular inputs keeps them.

    The settings of the model win on a conflict, since they were written for that
    model specifically.

    Cases expecting an error run last, whether common or belonging to the model, so
    that a worker still loading after a restart answers a real request first
    rather than holding up a refusal.

    Args:
        common_cases (list): `common_test_cases` from the configuration.
        own_cases (list): The cases of the model, or a lone default.

    Returns:
        cases (list): Cases to run, scenarios first and refusals last.
    """

    shared_expect, shared_validators, scenarios = {}, {}, []

    # What the model sends by default, which a common scenario varies rather
    # than replaces: a model that has to be given particular files (Matchering
    # needs two different recordings) still needs them in every scenario
    base = own_cases[0] if own_cases else {}

    for case in common_cases:
        if any(case.get(key) for key in INPUT_KEYS):
            scenario = dict(case, name=f"common:{case.get('name', 'unnamed')}")
            for key in INPUT_KEYS:
                if key not in case and base.get(key):
                    scenario[key] = base[key]
            scenarios.append(scenario)
            continue
        # Merged per output, so that two common cases targeting the same
        # output (e.g. "*") both keep their rules
        for label, rules in (case.get("expect") or {}).items():
            shared_expect.setdefault(label, {}).update(rules or {})
        shared_validators.update(case.get("validators") or {})

    merged = []
    for case in own_cases:
        case = dict(case)
        # A refusal returns no outputs, so there is nothing to check in it
        if case.get("expect_error"):
            merged.append(case)
            continue
        # The rules of the model are applied over the shared ones, per output
        # for `expect` so that a shared rule on one output survives a model
        # rule on another
        expect = {label: dict(rules or {})
                  for label, rules in shared_expect.items()}
        for label, rules in (case.get("expect") or {}).items():
            expect.setdefault(label, {}).update(rules or {})
        if expect:
            case["expect"] = expect
        validators = dict(shared_validators)
        validators.update(case.get("validators") or {})
        if validators:
            case["validators"] = validators
        merged.append(case)

    cases = scenarios + merged
    return ([case for case in cases if not case.get("expect_error")]
            + [case for case in cases if case.get("expect_error")])


def readme_problems(readme: dict | None, card: dict) -> str:
    """
    Where a Space's README disagrees with the model card /controls returns.

    HARP lists a sleeping model on the Home tab from its README alone, and
    shows the model card once the model is loaded, so the two have to agree.
    docs/DEPLOYMENT.md asks for the README's tags to be the full tag list of the
    card and its title to be the name of the card, and nothing else reconciles
    them.

    Args:
        readme (dict | None): The README's `tags` and `title`, or None when
            there is no README to compare (a local example).
        card (dict): The model card from /controls.

    Returns:
        problems (str): What disagrees, or empty when the two agree.
    """

    if readme is None:
        return ""

    problems = []
    declared, carried = set(readme.get("tags") or []), set(card.get("tags") or [])

    if carried - declared:
        problems.append(f"the README lacks the card's tags {sorted(carried - declared)}")
    if declared - carried:
        problems.append(f"the README declares tags the card does not carry "
                        f"{sorted(declared - carried)}")

    title, name = readme.get("title"), card.get("name")
    if title and name and title != name:
        problems.append(f"the README title '{title}' is not the card's name '{name}'")

    return "; ".join(problems)


def run_endpoint_tests(client: Client, result: ModelResult, assets: Assets,
                       overrides: dict, opts: argparse.Namespace,
                       readme: dict | None = None) -> ModelResult:
    """
    Verify /controls and run every configured /process test case.

    Each case runs inside its own error boundary, so a failing case does not
    stop the remaining cases.

    Args:
        client (Client): Connected gradio client for the deployment.
        result (ModelResult): Result record to complete.
        assets (Assets): Synthesized input files.
        overrides (dict): The entry of this model in config.yml `overrides`.
        opts (argparse.Namespace): Parsed command-line options.
        readme (dict | None): The Space's README `tags` and `title`, checked
            against the model card, or None for a model with no README.

    Returns:
        result (ModelResult): The same record, completed.
    """

    # ZeroGPU models default to a lower process timeout, since their jobs are
    # short once running. A per-model process_timeout override still wins.
    default_process_timeout = (opts.zerogpu_process_timeout
                               if result.zerogpu
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

    # Free to check, so a load-only run checks it too
    problems = readme_problems(readme, controls.get("card", {}))
    readme_case = CaseResult(name="readme", ok=False, error=problems) if problems else None

    if load_only:
        if readme_case:
            result.cases.append(readme_case)
            result.error = f"[readme] {problems}"
        else:
            result.status = PASS
        return result

    # --- /process test cases -------------------------------------------------
    # Common cases (generic, applied to every model) are merged into this
    # cases of the model, or run beside them when they send inputs of their own
    # (see merge_common_cases). A model can opt out of all of them with
    # `skip_common_cases: true`, or of the ones it names in a list.
    own_cases = overrides.get("test_cases") or [{"name": "default"}]
    skipped = overrides.get("skip_common_cases") or []
    common_cases = ([] if skipped is True else
                    [case for case in opts.common_test_cases or []
                     if case.get("name") not in skipped])
    cases = merge_common_cases(common_cases, own_cases)

    # A case sending an input to refuse applies only to a model that takes an
    # input of that kind, like a "*" rule that matches no output
    fed = fed_kinds(controls)
    cases = [case for case in cases
             if not (kinds := corrupted_kinds(assets, merge_specs(
                 overrides.get("synthesized_inputs"), case.get("synthesized_inputs"))))
             or kinds & fed]

    # --cases narrows the run to the cases it names, for one run
    wanted = getattr(opts, "cases", None)
    if wanted:
        cases = [case for case in cases
                 if case.get("name") in wanted
                 or case.get("name", "").removeprefix("common:") in wanted]

    for case in cases:
        result.cases.append(run_case(
            client, case, controls, assets, overrides, config_dir,
            process_timeout, connect_timeout))

    if readme_case:
        result.cases.append(readme_case)

    if wanted and not result.cases:
        result.error = f"skipped: no case named {', '.join(wanted)} applies"
        result.status = SKIP
    elif any(c.ok is False for c in result.cases):
        result.error = "; ".join(
            f"[{c.name}] {c.error}" for c in result.cases if c.ok is False)
        result.status = FAIL
    elif not any(c.ok for c in result.cases):
        # Every case was skipped, so /process was never called. Passing here
        # would vouch for the model on the strength of /controls alone, which
        # is the thing these cases exist to go beyond. It reports as a skip
        # rather than a failure because the gap is in the configuration, not
        # the deployment: a model whose inputs cannot be synthesized needs a
        # `files` entry naming real ones.
        result.error = "; ".join(f"[{c.name}] {c.error}" for c in result.cases)
        result.status = SKIP
    else:
        result.status = PASS

    return result
