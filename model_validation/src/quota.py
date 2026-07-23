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
    Accumulates the ZeroGPU processing time consumed during a run.

    The total is the cumulative /process wall time across ZeroGPU models
    (CPU and dedicated-hardware models never contribute). It is an upper
    bound on the GPU seconds charged, since wall time includes queue time,
    and is shown against an optional budget (`zerogpu_budget_seconds` in
    config.yml).
    """

    def __init__(self, budget: float | None):
        self.budget = budget
        self.used = 0.0
        self._lock = threading.Lock()

    def add(self, seconds: float) -> None:
        """
        Record ZeroGPU processing time from a completed model validation.

        Args:
            seconds (float): Wall time spent in this model's /process calls.
        """

        with self._lock:
            self.used += seconds

    def summary(self) -> str:
        """
        Format the ZeroGPU time consumed so far for a console line.

        Returns:
            summary (str): e.g. "ZeroGPU time ~38s/1500s budget this run".
        """

        with self._lock:
            used = int(self.used)

        if self.budget:
            return f"ZeroGPU time ~{used}s/{int(self.budget)}s budget this run"
        return f"ZeroGPU time ~{used}s this run"
