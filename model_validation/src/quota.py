"""
ZeroGPU usage tracking for HARP model validation.

ZeroGPU allowances are consumed per account, so a long validation run can eat
into the day's quota. There is no documented public API for the remaining
account quota, so rather than guess at it this module tracks the ZeroGPU time
*this run* consumes - the part attributable to validation - and reports it
after every ZeroGPU model.
"""

import threading


__all__ = [
    'ZeroGPUTracker',
    'ZeroGPUQuotaGuard',
    'is_zerogpu'
]


def is_zerogpu(hardware: str) -> bool:
    """
    Whether a space's hardware consumes ZeroGPU quota.

    ZeroGPU hardware ids start with "zero" (e.g. "zero-a10g"); anything
    else (cpu-basic, t4-small, ...) does not draw on the shared allowance.

    Args:
        hardware (str): HF hardware id, possibly empty.

    Returns:
        zerogpu (bool): True when the hardware is a ZeroGPU tier.
    """

    return bool(hardware) and hardware.lower().startswith("zero")


class ZeroGPUTracker:
    """
    Tracks ZeroGPU work done during a run.

    ZeroGPU bills dynamically: each call reserves its declared
    `@spaces.GPU(duration=...)` time up front, then refunds the unused portion
    once the function returns - so the account's usage rises during a run and
    settles lower afterwards, and the exact billed amount is not readable from
    the gradio client. Two figures are tracked instead:

      - the number of /process calls that reached the GPU - exact, and the
        most reliable signal of how much of the allowance a run will use;
      - the total /process wall time of those calls (queue plus execution) -
        an over-estimate of the settled bill, in the range of the mid-run
        reservation peak, not the amount that remains after refunds.

    Only calls that reached the GPU count; queued or input-skipped cases do
    not. CPU and dedicated-hardware models never contribute.
    """

    def __init__(self, budget: float | None):
        self.budget = budget
        self.calls = 0
        self.wall_seconds = 0.0
        self._lock = threading.Lock()

    def add(self, calls: int, wall_seconds: float) -> None:
        """
        Record ZeroGPU work from a completed model validation.

        Args:
            calls (int): Number of /process calls that ran on the GPU.
            wall_seconds (float): Total /process wall time of those calls.
        """

        with self._lock:
            self.calls += calls
            self.wall_seconds += wall_seconds

    def summary(self) -> str:
        """
        Format the ZeroGPU work done so far for a console line.

        Returns:
            summary (str): e.g.
                "ZeroGPU: 25 calls, ~340s wall (approx)".
        """

        with self._lock:
            calls, wall = self.calls, int(self.wall_seconds)

        plural = "s" if calls != 1 else ""
        if self.budget:
            detail = f"{calls} call{plural}, ~{wall}s/{int(self.budget)}s wall"
        else:
            detail = f"{calls} call{plural}, ~{wall}s wall"

        return f"ZeroGPU: {detail} (approx)"


class ZeroGPUQuotaGuard:
    """
    A shared, thread-safe flag tripped when the ZeroGPU allowance runs out.

    Spaces are validated concurrently. Once any ZeroGPU model reports its
    quota is exhausted, this guard causes the remaining ZeroGPU models to be
    skipped rather than run against an exhausted allowance.
    """

    def __init__(self):
        self._exhausted = False
        self._lock = threading.Lock()

    def mark_exhausted(self) -> None:
        """Record that the ZeroGPU allowance has been exhausted."""

        with self._lock:
            self._exhausted = True

    @property
    def exhausted(self) -> bool:
        """Whether the ZeroGPU allowance has been reported exhausted."""

        with self._lock:
            return self._exhausted
