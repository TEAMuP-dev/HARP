from __future__ import annotations

import importlib
import importlib.util
import json
import sys
import wave
from contextlib import contextmanager
from pathlib import Path
from types import ModuleType
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[2]
PYHARP_ROOT = REPO_ROOT / "pyharp"
REGISTRY_PATH = REPO_ROOT / "resources" / "models" / "model_registry.json"
DEFAULT_REPORT_DIR = REPO_ROOT / "artifacts" / "model_validation"


def ensure_repo_imports() -> None:
    pyharp_path = str(PYHARP_ROOT)

    if pyharp_path not in sys.path:
        sys.path.insert(0, pyharp_path)


def load_registry() -> dict[str, Any]:
    return json.loads(REGISTRY_PATH.read_text(encoding="utf-8"))


def resolve_repo_path(relative_path: str) -> Path:
    return REPO_ROOT / relative_path


def iter_enabled_validation_models() -> list[dict[str, Any]]:
    registry = load_registry()
    return [
        model
        for model in registry.get("models", [])
        if model.get("validation", {}).get("enabled", False)
    ]


def missing_python_modules(model_entry: dict[str, Any]) -> list[str]:
    missing: list[str] = []

    for module_name in model_entry.get("validation", {}).get("python_modules", []):
        if importlib.util.find_spec(module_name) is None:
            missing.append(module_name)

    return missing


@contextmanager
def suppress_gradio_launch() -> Any:
    original_launch = None
    gradio_blocks = None

    try:
        import gradio as gr

        gradio_blocks = gr.Blocks
        original_launch = gradio_blocks.launch
        gradio_blocks.launch = lambda self, *args, **kwargs: self
        yield
    finally:
        if gradio_blocks is not None and original_launch is not None:
            gradio_blocks.launch = original_launch


def import_app_module(model_entry: dict[str, Any]) -> ModuleType:
    ensure_repo_imports()

    module_path = resolve_repo_path(model_entry["validation"]["app_path"])
    module_name = f"harp_model_validation_{model_entry['id'].replace('-', '_')}"
    spec = importlib.util.spec_from_file_location(module_name, module_path)

    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to import app module at {module_path}.")

    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module

    with suppress_gradio_launch():
        spec.loader.exec_module(module)

    return module


def build_process_inputs(model_entry: dict[str, Any]) -> list[Any]:
    resolved_inputs: list[Any] = []

    for input_spec in model_entry.get("validation", {}).get("inputs", []):
        kind = input_spec["kind"]

        if kind in {"audio_fixture", "midi_fixture"}:
            resolved_inputs.append(str(resolve_repo_path(input_spec["path"])))
        elif kind == "literal":
            resolved_inputs.append(input_spec["value"])
        else:
            raise ValueError(f"Unsupported validation input kind: {kind}")

    return resolved_inputs


def normalize_outputs(result: Any) -> tuple[Any, ...]:
    if isinstance(result, tuple):
        return result

    if isinstance(result, list):
        return tuple(result)

    return (result,)


def validate_audio_file(output_path: Path, expected: dict[str, Any]) -> None:
    assert output_path.exists(), f"Audio output was not created: {output_path}"
    assert output_path.suffix == expected.get("extension", output_path.suffix)
    assert output_path.stat().st_size >= expected.get("min_size_bytes", 1)

    if output_path.suffix.lower() == ".wav":
        with wave.open(str(output_path), "rb") as wav_file:
            assert wav_file.getnframes() > 0
            assert wav_file.getframerate() > 0


def validate_midi_file(output_path: Path, expected: dict[str, Any]) -> None:
    assert output_path.exists(), f"MIDI output was not created: {output_path}"
    assert output_path.suffix == expected.get("extension", output_path.suffix)
    assert output_path.stat().st_size >= expected.get("min_size_bytes", 1)

    with output_path.open("rb") as midi_file:
        assert midi_file.read(4) == b"MThd"


def validate_label_list(output_value: Any) -> None:
    if hasattr(output_value, "meta") and hasattr(output_value, "labels"):
        assert output_value.meta.get("_type") == "pyharp.LabelList"
        return

    if isinstance(output_value, dict):
        assert output_value.get("meta", {}).get("_type") == "pyharp.LabelList"
        return

    raise AssertionError("Output was expected to be a LabelList-compatible object.")


def validate_outputs(result: Any, model_entry: dict[str, Any]) -> list[Path]:
    normalized_outputs = normalize_outputs(result)
    expected_outputs = model_entry.get("validation", {}).get("expected_outputs", [])

    assert len(normalized_outputs) == len(expected_outputs), (
        f"Expected {len(expected_outputs)} output(s) but got {len(normalized_outputs)}."
    )

    files_to_cleanup: list[Path] = []

    for output_value, expected in zip(normalized_outputs, expected_outputs):
        kind = expected["kind"]

        if kind == "audio_file":
            output_path = Path(output_value)
            validate_audio_file(output_path, expected)
            files_to_cleanup.append(output_path)
        elif kind == "midi_file":
            output_path = Path(output_value)
            validate_midi_file(output_path, expected)
            files_to_cleanup.append(output_path)
        elif kind == "label_list":
            validate_label_list(output_value)
        else:
            raise ValueError(f"Unsupported validation output kind: {kind}")

    return files_to_cleanup


def cleanup_outputs(paths: list[Path]) -> None:
    for output_path in paths:
        try:
            output_path.unlink(missing_ok=True)
        except OSError:
            pass


def summarize_reason(reason: str | None) -> str:
    if not reason:
        return ""

    lines = [line.strip() for line in str(reason).splitlines() if line.strip()]

    if not lines:
        return ""

    summary = lines[-1]

    for prefix in ("E   ", "Skipped: ", "AssertionError: "):
        if summary.startswith(prefix):
            summary = summary[len(prefix):]

    return summary.replace("|", "\\|")


def render_markdown_report(report: dict[str, Any]) -> str:
    results = sorted(report["results"], key=lambda item: (item["outcome"], item["id"]))
    lines = [
        "# HARP Model Validation",
        "",
        f"- Generated at: {report['generated_at']}",
        f"- Registry: `{report['registry_path']}`",
        f"- Total: {report['summary']['total']}",
        f"- Passed: {report['summary']['passed']}",
        f"- Failed: {report['summary']['failed']}",
        f"- Skipped: {report['summary']['skipped']}",
        "",
        "## Dashboard",
        "",
        "| Model ID | Name | Outcome | Detail |",
        "| --- | --- | --- | --- |"
    ]

    for result in results:
        lines.append(
            f"| `{result['id']}` | {result['name']} | {result['outcome']} | "
            f"{summarize_reason(result.get('reason'))} |"
        )

    return "\n".join(lines) + "\n"
