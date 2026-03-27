from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import pytest

from tests.model_validation.helpers import (
    DEFAULT_REPORT_DIR,
    REGISTRY_PATH,
    render_markdown_report,
)


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--run-network-validation",
        action="store_true",
        default=False,
        help="Run model validation cases that require network access.",
    )
    parser.addoption(
        "--model-validation-report-dir",
        action="store",
        default=None,
        help="Directory where model validation JSON/Markdown reports should be written.",
    )


def pytest_configure(config: pytest.Config) -> None:
    config._model_validation_results = {}


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item: pytest.Item, call: pytest.CallInfo[Any]) -> Any:
    outcome = yield
    report = outcome.get_result()

    if "model_entry" not in getattr(item, "fixturenames", ()):
        return

    if report.when not in {"setup", "call"}:
        return

    callspec = getattr(item, "callspec", None)

    if callspec is None:
        return

    model_entry = callspec.params.get("model_entry")

    if model_entry is None:
        return

    model_id = model_entry["id"]
    results = item.config._model_validation_results
    should_record = report.when == "call" or (report.when == "setup" and report.skipped)

    if not should_record:
        return

    reason = None

    if report.failed or report.skipped:
        reason = str(report.longrepr)

    results[model_id] = {
        "id": model_id,
        "name": model_entry["name"],
        "outcome": report.outcome,
        "reason": reason,
    }


def pytest_sessionfinish(session: pytest.Session, exitstatus: int) -> None:
    report_dir_option = session.config.getoption("--model-validation-report-dir")
    report_dir = Path(report_dir_option) if report_dir_option else DEFAULT_REPORT_DIR
    report_dir.mkdir(parents=True, exist_ok=True)

    results = list(session.config._model_validation_results.values())
    summary = {
        "total": len(results),
        "passed": sum(result["outcome"] == "passed" for result in results),
        "failed": sum(result["outcome"] == "failed" for result in results),
        "skipped": sum(result["outcome"] == "skipped" for result in results),
    }

    report = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "registry_path": str(REGISTRY_PATH),
        "exitstatus": exitstatus,
        "summary": summary,
        "results": results,
    }

    (report_dir / "latest.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    (report_dir / "latest.md").write_text(render_markdown_report(report), encoding="utf-8")
