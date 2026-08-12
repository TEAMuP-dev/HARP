"""
Validation driver for remote Hugging Face Spaces.

Gets a Space into a state where the harness can talk to it - waiting out a
build, restarting a crashed one, waking a sleeping one - then runs the
endpoint tests under the ZeroGPU concurrency and quota safeguards.
"""

import argparse
import contextlib
import time
import traceback

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
# Stages that resolve on their own if we wait (SLEEPING resolves on request)
TRANSIENT_STAGES = {"BUILDING", "RUNNING_BUILDING", "APP_STARTING", "SLEEPING"}
# Stages a restart can recover from; DELETING is excluded as there is nothing
# left to restart
RESTARTABLE_STAGES = {"BUILD_ERROR", "RUNTIME_ERROR", "CONFIG_ERROR",
                      "STOPPED", "PAUSED"}

RUNTIME_POLL_INTERVAL = 10   # cadence for polling a space's runtime stage


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
        time.sleep(RUNTIME_POLL_INTERVAL)
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
                time.sleep(CONNECT_RETRY_INTERVAL)
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
                # Now that the hardware is known, throttle on fact rather than
                # on the earlier assumption - a CPU model no longer queues
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
