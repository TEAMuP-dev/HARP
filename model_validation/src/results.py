"""
Result records and report generation for HARP model validation.
"""

import dataclasses
import json
import time
from pathlib import Path


__all__ = [
    'PASS',
    'FAIL',
    'SKIP',
    'CaseResult',
    'ModelResult',
    'status_emoji',
    'write_reports'
]


PASS, FAIL, SKIP = "PASS", "FAIL", "SKIP"


@dataclasses.dataclass
class CaseResult:
    """Outcome of a single /process test case."""

    name: str
    ok: bool | None = None    # None => skipped
    duration: float = 0.0     # total /process wall time (queue + execution)
    executed: bool = False    # the /process job left the queue and ran on GPU
    error: str = ""


@dataclasses.dataclass
class ModelResult:
    """Outcome of validating one model deployment."""

    target: str                       # space id or "examples/<example>"
    kind: str = "space"               # "space" | "local"
    status: str = FAIL
    stage: str = ""                   # HF runtime stage, or "LOCAL"
    hardware: str = ""                # HF hardware (e.g. "zero-a10g", "cpu-basic")
    controls_ok: bool = False
    cases: list = dataclasses.field(default_factory=list)
    duration: float = 0.0
    error: str = ""
    model_name: str = ""              # from the model card


def status_emoji(result: ModelResult) -> str:
    """
    Symbol used for a result in console output and reports.

    Args:
        result (ModelResult): The result to represent.

    Returns:
        emoji (str): One of the pass/fail/skip symbols.
    """

    return {PASS: "✅", FAIL: "❌", SKIP: "⏭️"}[result.status]


def write_reports(results: list, out_dir: Path, label: str,
                  command: str = "", options: dict | None = None) -> None:
    """
    Write the machine-readable and human-readable reports.

    Args:
        results (list): ModelResult objects for every validated model.
        out_dir (Path): Directory receiving report.json and report.md.
        label (str): Report heading (e.g. "teamup-tech spaces").
        command (str): The command line the run was invoked with.
        options (dict | None): The resolved run options, recorded in the
            report so a run's parameters are reproducible from it.
    """

    out_dir.mkdir(parents=True, exist_ok=True)

    passed = sum(r.status == PASS for r in results)
    failed = sum(r.status == FAIL for r in results)
    skipped = sum(r.status == SKIP for r in results)
    validated = len(results) - skipped   # denominator excludes skipped models

    payload = {
        "label": label,
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "command": command,
        "options": options or {},
        "total": len(results),
        "validated": validated,
        "passed": passed,
        "failed": failed,
        "skipped": skipped,
        "results": [dataclasses.asdict(r) for r in results],
    }
    (out_dir / "report.json").write_text(json.dumps(payload, indent=2))

    headline = f"**{passed}/{validated} models passed**"
    if skipped:
        headline += f", {skipped} skipped"
    lines = [
        f"# HARP Model Validation Report - {label}",
        "",
        f"{headline} ({payload['timestamp']})",
        "",
        f"Command: `{command}`" if command else "",
        "",
        "| Model | Status | Stage | Hardware | Controls | Cases | Time (s) | Detail |",
        "|---|---|---|---|---|---|---|---|",
    ]

    # Failures first, then alphabetical, so problems are visible at a glance
    for r in sorted(results, key=lambda r: (r.status != FAIL, r.target)):
        if r.cases:
            cases = ", ".join(
                f"{c.name} {'✅' if c.ok else '⏭️' if c.ok is None else '❌'}"
                for c in r.cases)
        else:
            cases = "—"
        link = (f"[{r.target}](https://huggingface.co/spaces/{r.target})"
                if r.kind == "space" else f"`{r.target}`")
        detail = r.error.replace("|", "\\|")[:300] if r.error else ""
        lines.append(f"| {link} | {status_emoji(r)} {r.status} | {r.stage} "
                     f"| {r.hardware or '—'} | {'✅' if r.controls_ok else '❌'} "
                     f"| {cases} | {r.duration} | {detail} |")

    (out_dir / "report.md").write_text("\n".join(lines) + "\n")
