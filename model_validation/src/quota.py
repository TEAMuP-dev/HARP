"""
ZeroGPU usage tracking for HARP model validation.

ZeroGPU allowances are consumed per account, so a long validation run draws on
the day's quota. Hugging Face publishes no reliable API for the remaining
allowance, so this module reports the work attributable to the run itself.
"""

import threading


__all__ = [
    'ZeroGPUTracker',
    'is_zerogpu'
]


def is_zerogpu(hardware: str) -> bool:
    """
    Whether a space's hardware consumes ZeroGPU quota.

    ZeroGPU hardware ids start with "zero", such as "zero-a10g". Anything
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

    ZeroGPU bills dynamically. Each call reserves its declared
    `@spaces.GPU(duration=...)` time up front, then refunds the unused portion
    once the function returns, so an account's usage rises during a run and
    settles lower afterwards. What was actually billed cannot be read from the
    gradio client, so two figures are tracked instead.

    The number of /process calls that reached the GPU is exact. Since each one
    reserves allowance, it is the most reliable signal of how much of the
    allowance a run uses.

    The total wall time of those calls, measured from submission to result, is
    a rough ceiling on the GPU time rather than the bill. It includes the queue
    wait, which is not GPU time, and it predates the refund described above.

    Only calls that reached the GPU count. Cases that were skipped for want of
    an input, or that never left the queue, do not, and CPU and
    dedicated-hardware models never contribute.
    """

    def __init__(self):
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

        return (f"ZeroGPU: {calls} call{'s' if calls != 1 else ''}, "
                f"~{wall}s wall (approx)")
