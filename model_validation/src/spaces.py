"""
Validation driver for remote Hugging Face Spaces.

Gets a Space into a state where the harness can talk to it, waiting out a
build, restarting a crashed one, or waking a sleeping one. It then runs the
endpoint tests under the ZeroGPU concurrency and quota safeguards.
"""

import argparse
import contextlib
import inspect
import json
import time
import traceback

import httpx
from gradio_client import Client

from assets import Assets
from harness import (run_endpoint_tests, close_client, is_quota_exhausted,
                     CONNECT_RETRY_INTERVAL)
from quota import spends_zerogpu
from results import ModelResult, FAIL, SKIP
from utils import run_with_timeout, scrub, describe_exception


__all__ = [
    'test_space'
]


# Stages in which a space answers requests: RUNNING serves immediately, SLEEPING
# wakes on the first request
SERVABLE_STAGES = {"RUNNING", "SLEEPING"}
# Stages that resolve on their own if we wait. SLEEPING is not one of them,
# since a sleeping space only starts once something asks it to (see wake_space).
# RUNNING_APP_STARTING is a new revision starting while the old one serves, so
# waiting it out validates the revision just deployed
TRANSIENT_STAGES = {"BUILDING", "RUNNING_BUILDING", "APP_STARTING",
                    "RUNNING_APP_STARTING"}
# Stages from which a restart can recover. DELETING is excluded, as there is
# nothing left to restart
RESTARTABLE_STAGES = {"BUILD_ERROR", "RUNTIME_ERROR", "CONFIG_ERROR",
                      "STOPPED", "PAUSED"}

# gradio_client renamed the Client's token argument from hf_token to token in
# 2.0, and both spellings are in use, so ask this install which it accepts
TOKEN_KWARG = ("token"
               if "token" in inspect.signature(Client.__init__).parameters
               else "hf_token")

RUNTIME_POLL_INTERVAL = 10   # cadence for polling the runtime stage of a space
WAKE_MIN_TIMEOUT = 60        # never give a wake request less than this
WAKE_ATTEMPTS = 3            # asks before giving up on a space that stays asleep
CONNECT_RESERVE = 60         # budget held back from the wake, to connect with

# How much of the run log of a failed space the report carries, and for how long it
# is read. The log is a live stream that never ends, so it is read for a fixed
# time: Hugging Face replays the recent history first, then waits for more.
LOG_TAIL_LINES = 25
LOG_LISTEN_SECONDS = 8

# Shutdown noise that nearly every space logs, from asyncio closing an event
# loop, which says nothing about why a case failed
LOG_NOISE = ("Invalid file descriptor", "BaseEventLoop.__del__", "Exception ignored in",
             "selectors.py", "_close_self_pipe", "_remove_reader", "_fileobj_to_fd",
             "_fileobj_lookup", "return mapping[fileobj]", "key = self._selector")


def wait_for_runtime(api, space_id: str, deadline: float,
                     unsettled: set = TRANSIENT_STAGES):
    """
    Poll the runtime of a space until its stage settles or the deadline passes.

    Args:
        api (HfApi): Authenticated Hugging Face API client.
        space_id (str): The space to poll.
        deadline (float): time.time() value at which to stop polling.
        unsettled (set): Stages still considered to be in progress. SLEEPING
            belongs here only once the space has been asked to start, since
            otherwise it would never resolve.

    Returns:
        runtime (SpaceRuntime): The last observed runtime (stage, hardware).
    """

    runtime = api.get_space_runtime(space_id)

    while runtime.stage in unsettled and time.time() < deadline:
        time.sleep(RUNTIME_POLL_INTERVAL)
        runtime = api.get_space_runtime(space_id)

    return runtime


def resolve_host(api, space_id: str) -> tuple:
    """
    Look up the URL from which a space serves.

    Kept apart from the wake request itself so a caller retrying the wake can
    reuse a host it already resolved, since the host does not change within
    a run.

    Args:
        api (HfApi): Authenticated Hugging Face API client.
        space_id (str): The space whose host to find.

    Returns:
        host (str | None): The host URL of the space, or None when the lookup
            failed.
        problem (str): Why the lookup failed, or empty. A failed lookup means
            no wake request can go out at all, which is the difference between
            a space that would not start and one that was never asked.
    """

    try:
        return api.space_info(space_id).host, ""
    except Exception as exc:  # noqa: BLE001 - reported to the caller
        return None, f"could not look up its host: {describe_exception(exc)}"


def space_metadata(api, space_id: str) -> tuple:
    """
    Look up the host of a space and the tags and title in its README at once.

    One request serves both the wake, which needs the host, and the README
    check, so a run makes one fewer call to the Hub per space, which matters
    late in a run, when the Hub starts to rate-limit it.

    Args:
        api (HfApi): Authenticated Hugging Face API client.
        space_id (str): The space whose host and README to read.

    Returns:
        host (str | None): The host URL of the space, or None when the lookup
            failed, in which case the wake resolves it again.
        readme (dict | None): Its `tags` (a list) and `title`, or None when
            the lookup failed, so that nothing is compared against a README
            that was never read.
    """

    try:
        info = api.space_info(space_id)
    except Exception:  # noqa: BLE001 - nothing to compare is the safe reading
        return None, None

    card = info.card_data

    return info.host, {"tags": list(getattr(card, "tags", None) or []),
                       "title": getattr(card, "title", None)}


def wake_space(host: str, timeout: float) -> tuple:
    """
    Ask a sleeping space to start, and wait for it to answer.

    Hugging Face holds a request to a sleeping space open while the app comes
    up, answering it once the app serves, so one request both triggers the
    start and waits for it. The timeout therefore has to cover a cold start of
    a minute or more. gradio_client cannot be used for this, since it fetches
    the config with httpx's 5 second default and abandons the request long
    before the space can answer.

    Args:
        host (str): The host URL of the space (see resolve_host).
        timeout (float): Seconds to hold the request open.

    Returns:
        serving (bool): True when the space answered, meaning it is serving.
        problem (str): What stopped the request, or empty. A request that goes
            out and times out is ordinary and reports nothing, since the space
            is simply still starting.
    """

    try:
        answer = httpx.get(f"{host.rstrip('/')}/config", timeout=timeout)
        if answer.is_success:
            return True, ""
        # An answer this quick is a refusal rather than a start. Hugging Face
        # holds the request open while an app boots, so a status instead of a
        # wait means it declined to boot one
        return False, f"the platform answered {answer.status_code} rather than starting it"
    except httpx.TimeoutException:
        return False, ""
    except Exception as exc:  # noqa: BLE001 - reported to the caller
        return False, f"the wake request failed: {describe_exception(exc)}"


def start_space(api, space_id: str, deadline: float, host: str | None = None) -> tuple:
    """
    Get a sleeping space started.

    The ask is repeated rather than made once. A space that is still SLEEPING
    was never successfully asked, whatever the reason, and one failed lookup
    late in a run would otherwise cost the whole connect budget waiting for
    something nobody requested.

    Args:
        api (HfApi): Authenticated Hugging Face API client.
        space_id (str): The space to start.
        deadline (float): time.time() value at which to stop asking.
        host (str | None): The host URL of the space, when already known, or
            None to resolve it here.

    Returns:
        stage (str): The stage the space reached.
        problem (str): The last reason an ask could not be made, or empty.
    """

    problem = ""

    for attempt in range(WAKE_ATTEMPTS):
        remaining = deadline - time.time()
        if remaining <= 0:
            break

        # The host does not change within a run, so a successful lookup is
        # reused and only a failed one is retried
        if host is None:
            host, problem = resolve_host(api, space_id)

        serving = False
        if host is not None:
            share = max(WAKE_MIN_TIMEOUT, remaining / (WAKE_ATTEMPTS - attempt))
            serving, problem = wake_space(host, min(share, remaining))

        stage = current_stage(api, space_id, "SLEEPING")

        if serving or stage != "SLEEPING":
            return stage, problem

        # A refusal comes back at once, so pause before asking again rather
        # than spending all the attempts inside a second
        if problem and time.time() + CONNECT_RETRY_INTERVAL < deadline:
            time.sleep(CONNECT_RETRY_INTERVAL)

    return current_stage(api, space_id, "SLEEPING"), problem


def current_stage(api, space_id: str, fallback: str) -> str:
    """
    Read the stage of a space for reporting, falling back to what we last saw.

    Args:
        api (HfApi): Authenticated Hugging Face API client.
        space_id (str): The space to read.
        fallback (str): Stage to report if the runtime cannot be read.

    Returns:
        stage (str): The current stage, or the fallback.
    """

    try:
        return api.get_space_runtime(space_id).stage
    except Exception:  # noqa: BLE001 - reporting must not raise
        return fallback


def recent_log_lines(space_id: str, token: str) -> list:
    """
    Read the end of the run log of a space, for the report on a failure.

    The cause of a failure is usually a traceback in the log of the space itself rather
    than anything the API returns, so the report carries the lines logged
    since the app last started, without the shutdown noise.

    Args:
        space_id (str): The space whose log to read.
        token (str): Hugging Face access token with access to the space.

    Returns:
        lines (list): Up to LOG_TAIL_LINES lines, oldest first, or an empty
            list when the log cannot be read.
    """

    lines = []
    deadline = time.time() + LOG_LISTEN_SECONDS

    try:
        with httpx.stream("GET", f"https://huggingface.co/api/spaces/{space_id}/logs/run",
                          headers={"Authorization": f"Bearer {token}"},
                          timeout=httpx.Timeout(LOG_LISTEN_SECONDS, read=2.0)) as response:
            if response.status_code != 200:
                return []
            for raw in response.iter_lines():
                if raw.startswith("data:"):
                    try:
                        text = json.loads(raw[len("data:"):])["data"]
                    except (ValueError, KeyError, TypeError):
                        text = ""
                    for line in text.splitlines():
                        if "===== Application Startup" in line:
                            lines = []
                        if line.strip() and not any(noise in line for noise in LOG_NOISE):
                            lines.append(line.rstrip())
                if time.time() > deadline:
                    break
    except Exception:  # noqa: BLE001 - a read timeout is how the replay ends,
        pass           # and a diagnostic must never fail the run

    return lines[-LOG_TAIL_LINES:]


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
        overrides (dict): The entry of this space in config.yml `overrides`.
        quota_exhausted (threading.Event | None): Shared flag. When set,
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

    def follow_runtime(deadline: float):
        """Wait for the stage to settle, mirroring it onto the result."""
        runtime = wait_for_runtime(api, space_id, deadline)
        result.stage = runtime.stage
        return runtime

    try:
        # --- Runtime stage (restart crashed/stopped spaces by default) -------
        deadline = time.time() + connect_timeout
        runtime = follow_runtime(deadline)
        stage = runtime.stage
        # requested_hardware reflects the configuration of the space even while
        # it is sleeping or stopped (hardware itself is only set when live)
        result.hardware = runtime.requested_hardware or runtime.hardware or ""
        host, readme = space_metadata(api, space_id)
        tags = (readme or {}).get("tags") or []
        zerogpu = result.zerogpu = spends_zerogpu(result.hardware, tags)
        # Hardware is occasionally unreported for a sleeping or starting space.
        # Throttle those as if they were ZeroGPU: being wrong only costs run
        # time, whereas leaving a real ZeroGPU model unthrottled overlaps GPU
        # reservations. Skipping, by contrast, stays strict, since wrongly
        # skipping a CPU model would silently drop it from validation.
        throttle = zerogpu or not result.hardware

        # Skip ZeroGPU models before doing any work that would spend quota
        if zerogpu and opts.skip_zerogpu:
            result.status = SKIP
            result.error = "skipped: spends ZeroGPU allowance (--skip-zerogpu)"
            return result
        if zerogpu and quota_exhausted is not None and quota_exhausted.is_set():
            result.status = SKIP
            result.error = "skipped: ZeroGPU allowance already exhausted this run"
            return result

        if stage not in SERVABLE_STAGES:
            if opts.restart_failed and stage in RESTARTABLE_STAGES:
                print(f"  [{space_id}] stage={stage}, requesting restart...")
                try:
                    api.restart_space(space_id)
                except Exception as exc:  # noqa: BLE001 - e.g. read-only token
                    result.error = scrub(
                        f"space is not running (stage={stage}) and restart "
                        f"failed: {describe_exception(exc)}", token)
                    return result
                stage = follow_runtime(time.time() + connect_timeout).stage
            if stage not in SERVABLE_STAGES:
                result.error = f"space is not running (stage={stage})"
                return result

        # --- Wake, then wait for the app to come up -------------------------
        # A sleeping space is servable but not yet serving. Ask it to start and
        # then follow its stage, rather than inferring liveness from repeated
        # connection attempts, which cannot tell "still starting" from "broken"
        deadline = time.time() + connect_timeout
        wake_problem = ""
        if stage == "SLEEPING":
            stage, wake_problem = start_space(api, space_id,
                                              deadline - CONNECT_RESERVE, host)
            result.stage = stage
        if stage == "SLEEPING" and wake_problem:
            # A space stuck this way does not answer requests, so fall back to
            # the documented recovery for it, which is a restart
            if opts.restart_failed:
                print(f"  [{space_id}] would not wake, requesting restart...")
                try:
                    api.restart_space(space_id)
                except Exception as exc:  # noqa: BLE001 - e.g. read-only token
                    raise RuntimeError(
                        f"space would not start: {wake_problem}, and the "
                        f"restart failed: {describe_exception(exc)}") from None
                # Give the stage a moment to reflect the restart before
                # following it, since it reads as SLEEPING for a beat
                time.sleep(RUNTIME_POLL_INTERVAL)
                stage = follow_runtime(deadline).stage

            if stage == "SLEEPING":
                raise RuntimeError(f"space would not start: {wake_problem}")
        if stage in TRANSIENT_STAGES:
            stage = follow_runtime(deadline).stage

        # --- Connect (retrying while the app finishes serving) --------------
        last_exc = None
        while time.time() < deadline:
            try:
                client = run_with_timeout(
                    lambda: Client(space_id, verbose=False,
                                   **{TOKEN_KWARG: token}),
                    max(10.0, deadline - time.time()), "connect")
                break
            except TypeError:
                # The call itself is wrong rather than the space, so retrying
                # would burn the whole connect budget to no purpose
                raise
            except Exception as exc:  # noqa: BLE001 - space may still be waking
                last_exc = exc
                time.sleep(CONNECT_RETRY_INTERVAL)
        if client is None:
            # Report where the space actually ended up, which separates one
            # still coming up from one that never started at all
            result.stage = current_stage(api, space_id, result.stage)
            detail = last_exc or f"space was {result.stage} when time ran out"
            if wake_problem:
                detail = f"{detail} ({wake_problem})"
            raise RuntimeError(f"could not connect: {detail}")
        # The space is serving now, so record that rather than the stale stage
        # seen before it woke, and fill in the hardware if it was not reported
        # while the space was asleep
        if result.stage != "RUNNING":
            result.stage = "RUNNING"
        if not result.hardware:
            try:
                awake = api.get_space_runtime(space_id)
                result.hardware = awake.hardware or awake.requested_hardware or ""
                # Now that the hardware is known, throttle on fact rather than
                # on the earlier assumption, so a CPU model no longer queues
                # behind ZeroGPU ones
                zerogpu = result.zerogpu = spends_zerogpu(result.hardware, tags)
                throttle = zerogpu or not result.hardware
            except Exception:  # noqa: BLE001 - hardware stays unknown
                pass

        # Serialize ZeroGPU models: concurrent GPU calls each reserve their
        # declared duration, so overlapping them ties up allowance that is not
        # being used
        limiter = zerogpu_limiter if (throttle and zerogpu_limiter is not None) \
            else contextlib.nullcontext()
        with limiter:
            # The allowance may have run out while waiting for a slot
            if zerogpu and quota_exhausted is not None and quota_exhausted.is_set():
                result.status = SKIP
                result.error = "skipped: ZeroGPU allowance already exhausted this run"
                return result

            run_endpoint_tests(client, result, assets, overrides, opts, readme)

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
        if result.status == FAIL and not is_quota_exhausted(result.error):
            result.log_tail = recent_log_lines(space_id, token)
