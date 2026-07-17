"""
ZeroGPU quota tracking for HARP model validation.

ZeroGPU allowances are consumed per account, so a long validation run can
exhaust the day's quota partway through. To make that visible, the quota
state is reported at the start of a run and after every model.
"""

import json
import threading
import urllib.request


__all__ = [
    'QuotaTracker',
    'fetch_account_quota',
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


def fetch_account_quota(token: str) -> str | None:
    """
    Best-effort fetch of the account's quota state from huggingface.co.

    Hugging Face has no *documented* public API for ZeroGPU quota; /api/quota
    exists but its schema is unstable, so parse defensively: surface any
    entries whose keys mention gpu/zero and return None when nothing useful
    comes back. Never raises.

    Args:
        token (str): Hugging Face access token.

    Returns:
        summary (str | None): Compact "key=value" summary of GPU-related
            quota entries, or None when unavailable.
    """

    if not token:
        return None

    try:
        req = urllib.request.Request(
            "https://huggingface.co/api/quota",
            headers={"Authorization": f"Bearer {token}"})
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read().decode())
    except Exception:  # noqa: BLE001 - quota reporting must never break a run
        return None

    def gpu_entries(obj, prefix=""):
        found = []
        if isinstance(obj, dict):
            for key, value in obj.items():
                name = f"{prefix}{key}"
                if any(s in key.lower() for s in ("zero", "gpu")) and \
                        isinstance(value, (int, float, str)):
                    found.append(f"{name}={value}")
                else:
                    found += gpu_entries(value, f"{name}.")
        elif isinstance(obj, list):
            for value in obj:
                found += gpu_entries(value, prefix)
        return found

    entries = gpu_entries(data)

    return ", ".join(entries[:4]) if entries else None


class QuotaTracker:
    """
    Tracks ZeroGPU usage during a validation run.

    Two signals are combined into each status line:
        - cumulative /process wall time this run on ZeroGPU spaces only
          (CPU-hardware models do not draw on the allowance and are never
          counted); an upper bound on GPU seconds consumed, since it
          includes queue time, shown against an optional budget
          (`zerogpu_budget_seconds` in config.yml);
        - the account quota reported by huggingface.co, when available.
    """

    def __init__(self, token: str, budget: float | None):
        self.token = token
        self.budget = budget
        self.used = 0.0
        self._lock = threading.Lock()

    def add(self, seconds: float) -> None:
        """
        Record processing time consumed by a completed model validation.

        Args:
            seconds (float): Wall time spent in /process calls.
        """

        with self._lock:
            self.used += seconds

    def status(self) -> str:
        """
        Format the current quota state for a console line.

        Returns:
            status (str): e.g. "[GPU time ~38s/1500s budget | left=120s]".
        """

        with self._lock:
            used = int(self.used)

        if self.budget:
            usage = f"GPU time ~{used}s/{int(self.budget)}s budget"
        else:
            usage = f"GPU time ~{used}s this run"

        account = fetch_account_quota(self.token)

        return f"[{usage}]" if account is None else f"[{usage} | {account}]"
