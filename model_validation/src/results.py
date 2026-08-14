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
    'case_emoji',
    'write_reports'
]


PASS, FAIL, SKIP = "PASS", "FAIL", "SKIP"


@dataclasses.dataclass
class CaseResult:
    """Outcome of a single /process test case."""

    name: str
    ok: bool | None = None    # None => skipped
    duration: float = 0.0     # total /process wall time (queue + execution)
    gpu_calls: int = 0        # /process calls that reached the GPU, each of
                              # which reserves ZeroGPU allowance. A retried
                              # call reserves another time and counts again
    error: str = ""


@dataclasses.dataclass
class ModelResult:
    """Outcome of validating one model deployment."""

    target: str                       # space id or "examples/<example>"
    kind: str = "space"               # "space" | "local"
    status: str = FAIL                # default, so any early return with an
                                      # error set is recorded as a failure
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


def case_emoji(ok: bool | None) -> str:
    """
    Symbol for one test case's outcome.

    Args:
        ok (bool | None): True passed, False failed, None skipped.

    Returns:
        emoji (str): The corresponding symbol.
    """

    return {True: "✅", False: "❌", None: "⏭️"}[ok]


def write_reports(results: list, out_dir: Path, command: str = "") -> dict:
    """
    Write the machine-readable and human-readable reports.

    Args:
        results (list): ModelResult objects for every validated model.
        out_dir (Path): Directory receiving report.json and report.md.
        command (str): The command line the run was invoked with, which is
            what makes the run reproducible from the report alone.

    Returns:
        payload (dict): The report.json contents, whose counts the caller
            reuses so the console summary cannot disagree with the report.
    """

    out_dir.mkdir(parents=True, exist_ok=True)

    passed = sum(r.status == PASS for r in results)
    failed = sum(r.status == FAIL for r in results)
    skipped = sum(r.status == SKIP for r in results)
    validated = len(results) - skipped   # denominator excludes skipped models

    payload = {
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "command": command,
        "total": len(results),
        "validated": validated,
        "passed": passed,
        "failed": failed,
        "skipped": skipped,
        "results": [dataclasses.asdict(r) for r in results],
    }
    (out_dir / "report.json").write_text(json.dumps(payload, indent=2))

    (out_dir / "report.md").write_text(render_markdown(results, payload))

    return payload


def render_markdown(results: list, payload: dict) -> str:
    """
    Render the human-readable report.

    The table carries only the per-model facts that fit a uniform row. Error
    text is reported once, in the section below it, where it can be shown in
    full.

    Args:
        results (list): ModelResult objects for every validated model.
        payload (dict): The report.json payload (headline counts, command).

    Returns:
        markdown (str): The rendered report.
    """

    # Failures first, then alphabetical, so problems are visible at a glance
    ranked = sorted(results, key=lambda r: (r.status != FAIL, r.target))

    headline = f"**{payload['passed']}/{payload['validated']} models passed**"
    if payload["skipped"]:
        headline += f", {payload['skipped']} skipped"

    lines = ["# HARP Model Validation Report", ""]
    lines += [f"{headline} ({payload['timestamp']})", ""]
    if payload["command"]:
        lines += [f"Command: `{payload['command']}`", ""]
    lines += ["| Model | Status | Stage | Hardware | Controls | Cases | Time (s) |",
              "|---|---|---|---|---|---|---|"]

    for r in ranked:
        cases = ", ".join(f"{c.name} {case_emoji(c.ok)}" for c in r.cases) or "-"
        link = (f"[{r.target}](https://huggingface.co/spaces/{r.target})"
                if r.kind == "space" else f"`{r.target}`")
        lines.append(f"| {link} | {status_emoji(r)} {r.status} | {r.stage} "
                     f"| {r.hardware or '-'} | {'✅' if r.controls_ok else '❌'} "
                     f"| {cases} | {r.duration} |")

    # Everything that reported text, including a skipped case on an otherwise
    # passing model. A model-level error is the joined case errors whenever
    # there are cases, so it is only printed when there are none.
    problems = [r for r in ranked
                if r.error or r.status == FAIL or any(c.error for c in r.cases)]
    if problems:
        lines += ["", "## Details", ""]
        for r in problems:
            lines += [f"### {r.target} ({r.status})", ""]
            if r.error and not r.cases:
                lines += ["```", r.error, "```", ""]
            for c in r.cases:
                if c.error:
                    lines += [f"- **{c.name}** {case_emoji(c.ok)}", "",
                              "```", c.error, "```", ""]

    return "\n".join(lines) + "\n"
