"""
Validation driver for remote Hugging Face Spaces.

Gets a Space into a state where the harness can talk to it, waiting out a
build, restarting a crashed one, or waking a sleeping one. It then runs the
endpoint tests under the ZeroGPU concurrency and quota safeguards.
"""

import argparse
import contextlib
import time
import traceback

import httpx
from gradio_client import Client

from assets import Assets
from harness import (run_endpoint_tests, close_client, is_quota_exhausted,
                     CONNECT_RETRY_INTERVAL)
from quota import is_zerogpu
from results import ModelResult, FAIL, SKIP
from utils import run_with_timeout, scrub, describe_exception


__all__ = [
    'test_space'
]


# Stages a space can be talked to from: RUNNING serves immediately, SLEEPING
# wakes on the first request
SERVABLE_STAGES = {"RUNNING", "SLEEPING"}
# Stages that resolve on their own if we wait. SLEEPING is not one of them,
# since a sleeping space only starts once something asks it to (see wake_space)
TRANSIENT_STAGES = {"BUILDING", "RUNNING_BUILDING", "APP_STARTING"}
# Stages a restart can recover from. DELETING is excluded, as there is
# nothing left to restart
RESTARTABLE_STAGES = {"BUILD_ERROR", "RUNTIME_ERROR", "CONFIG_ERROR",
                      "STOPPED", "PAUSED"}

RUNTIME_POLL_INTERVAL = 10   # cadence for polling a space's runtime stage
WAKE_MIN_TIMEOUT = 60        # never give a wake request less than this
CONNECT_RESERVE = 60         # budget held back from the wake, to connect with


def wait_for_runtime(api, space_id: str, deadline: float,
                     unsettled: set = TRANSIENT_STAGES):
    """
    Poll a space's runtime until its stage settles or the deadline passes.

    Args:
        api (HfApi): Authenticated Hugging Face API client.
        space_id (str): The space to poll.
        deadline (float): time.time() value to stop polling at.
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


def wake_space(api, space_id: str, timeout: float) -> bool:
    """
    Ask a sleeping space to start, and wait for it to answer.

    Hugging Face holds a request to a sleeping space open while the app comes
    up, answering it once the app serves, so one request both triggers the
    start and waits it out. The timeout therefore has to cover a cold start of
    a minute or more. gradio_client cannot be used for this, since it fetches
    the config with httpx's 5 second default and abandons the request long
    before the space can answer.

    Args:
        api (HfApi): Authenticated Hugging Face API client.
        space_id (str): The space to wake.
        timeout (float): Seconds to hold the request open.

    Returns:
        serving (bool): True when the space answered, meaning it is serving.
    """

    try:
        host = api.space_info(space_id).host
        return httpx.get(f"{host.rstrip('/')}/config", timeout=timeout).is_success
    except Exception:  # noqa: BLE001 - a wake that does not answer is normal
        return False


def current_stage(api, space_id: str, fallback: str) -> str:
    """
    Read a space's stage for reporting, falling back to what we last saw.

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
        # reservations. Skipping, by contrast, stays strict, since wrongly
        # skipping a CPU model would silently drop it from validation.
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
                runtime = wait_for_runtime(api, space_id, time.time() + connect_timeout)
                stage = runtime.stage
                result.stage = stage
            if stage not in SERVABLE_STAGES:
                result.error = f"space is not running (stage={stage})"
                return result

        # --- Wake, then wait for the app to come up -------------------------
        # A sleeping space is servable but not yet serving. Ask it to start and
        # then follow its stage, rather than inferring liveness from repeated
        # connection attempts, which cannot tell "still starting" from "broken"
        deadline = time.time() + connect_timeout
        if stage == "SLEEPING":
            wake_budget = min(deadline - time.time(),
                              max(WAKE_MIN_TIMEOUT,
                                  deadline - time.time() - CONNECT_RESERVE))
            if not wake_space(api, space_id, wake_budget):
                # It did not answer in that window, so fall back to following
                # the stage. SLEEPING counts as in progress here, since the
                # start has now been requested
                runtime = wait_for_runtime(api, space_id, deadline,
                                           TRANSIENT_STAGES | {"SLEEPING"})
                stage = runtime.stage
                result.stage = stage

        # --- Connect (retrying while the app finishes serving) --------------
        last_exc = None
        while time.time() < deadline:
            try:
                client = run_with_timeout(
                    lambda: Client(space_id, hf_token=token, verbose=False),
                    max(10.0, deadline - time.time()), "connect")
                break
            except Exception as exc:  # noqa: BLE001 - space may still be waking
                last_exc = exc
                time.sleep(CONNECT_RETRY_INTERVAL)
        if client is None:
            # Report where the space actually ended up, which separates one
            # still coming up from one that never started at all
            result.stage = current_stage(api, space_id, result.stage)
            detail = last_exc or f"space was {result.stage} when time ran out"
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
                zerogpu = is_zerogpu(result.hardware)
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
