from __future__ import annotations

import importlib
import importlib.util
import json
import mimetypes
import os
import sys
import tempfile
import wave
from contextlib import contextmanager
from pathlib import Path
from types import ModuleType
from typing import Any
from urllib.parse import urlparse

import requests


REPO_ROOT = Path(__file__).resolve().parents[2]
PYHARP_ROOT = REPO_ROOT / "pyharp"
REGISTRY_PATH = REPO_ROOT / "resources" / "models" / "model_registry.json"
DEFAULT_REPORT_DIR = REPO_ROOT / "artifacts" / "model_validation"
NETWORK_VALIDATION_ENV = "HARP_ENABLE_NETWORK_VALIDATION"
AUDIO_FIXTURE_PATH = REPO_ROOT / "resources" / "media" / "test.wav"
MIDI_FIXTURE_PATH = REPO_ROOT / "resources" / "media" / "test.mid"
HUGGINGFACE_TOKEN_ENVS = ("HARP_HUGGINGFACE_TOKEN", "HF_TOKEN")
STABILITY_TOKEN_ENVS = ("HARP_STABILITY_API_KEY", "STABILITY_API_KEY")


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


def network_validation_enabled(pytest_config: Any) -> bool:
    return bool(
        pytest_config.getoption("--run-network-validation")
        or os.environ.get(NETWORK_VALIDATION_ENV) == "1"
    )


def get_required_env_value(model_entry: dict[str, Any]) -> str | None:
    env_name = model_entry.get("validation", {}).get("requires_env")

    if not env_name:
        return None

    return os.environ.get(env_name)


def get_first_env_value(names: tuple[str, ...]) -> str | None:
    for name in names:
        value = os.environ.get(name)

        if value:
            return value

    return None


def build_auth_headers(model_entry: dict[str, Any]) -> dict[str, str]:
    path = (model_entry.get("path") or "").lower()

    if path.startswith("stability/"):
        token = get_required_env_value(model_entry) or get_first_env_value(STABILITY_TOKEN_ENVS)
    else:
        token = get_first_env_value(HUGGINGFACE_TOKEN_ENVS)

    return {"Authorization": f"Bearer {token}"} if token else {}


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


def ensure_trailing_slash(url: str) -> str:
    return url if url.endswith("/") else url + "/"


def infer_gradio_endpoint_path(model_path: str) -> str:
    if model_path.startswith("http://localhost") or model_path.startswith("http://127.0.0.1"):
        return ensure_trailing_slash(model_path)

    if model_path.startswith("https://") and (".hf.space" in model_path or model_path.endswith(".gradio.live")):
        return ensure_trailing_slash(model_path)

    if model_path.startswith("https://huggingface.co/spaces/"):
        model_path = model_path.removeprefix("https://huggingface.co/spaces/")

    host, model = model_path.split("/", 1)
    return f"https://{host}-{model.replace('_', '-')}.hf.space/"


def post_gradio_call(endpoint: str,
                     request_type: str,
                     body: dict[str, Any],
                     headers: dict[str, str]) -> str:
    response = requests.post(
        endpoint + f"gradio_api/call/{request_type}",
        json=body,
        headers=headers,
        timeout=60,
    )
    response.raise_for_status()
    return response.json()["event_id"]


def wait_for_gradio_event(endpoint: str,
                          request_type: str,
                          event_id: str,
                          headers: dict[str, str]) -> list[Any]:
    with requests.get(
        endpoint + f"gradio_api/call/{request_type}/{event_id}",
        headers=headers,
        stream=True,
        timeout=(30, 180),
    ) as response:
        response.raise_for_status()

        current_event = ""

        for raw_line in response.iter_lines(decode_unicode=True):
            if not raw_line:
                continue

            line = raw_line.strip()

            if line.startswith("event:"):
                current_event = line.removeprefix("event:").strip().lower()
            elif line.startswith("data:"):
                payload = line.removeprefix("data:").strip()

                if current_event == "complete":
                    return json.loads(payload)

                if current_event == "error":
                    raise RuntimeError(f"Remote Gradio endpoint returned an error: {payload}")

        raise RuntimeError("Remote Gradio endpoint closed without a complete event.")


def upload_remote_file(endpoint: str,
                       local_path: Path,
                       headers: dict[str, str]) -> str:
    mime_type = mimetypes.guess_type(local_path.name)[0] or "application/octet-stream"

    with local_path.open("rb") as handle:
        response = requests.post(
            endpoint + "gradio_api/upload",
            headers=headers,
            files={"files": (local_path.name, handle, mime_type)},
            timeout=120,
        )

    response.raise_for_status()
    return response.json()[0]


def wrap_remote_file(remote_path: str) -> dict[str, Any]:
    return {
        "path": remote_path,
        "meta": {
            "_type": "gradio.FileData"
        }
    }


def normalize_dropdown_choice(choice: Any) -> Any:
    if isinstance(choice, list) and len(choice) == 2:
        return choice[1]

    return choice


def get_input_override(model_entry: dict[str, Any], label: str) -> Any:
    overrides = model_entry.get("validation", {}).get("input_overrides", {})

    if label not in overrides:
        return None

    value = overrides[label]

    if isinstance(value, str) and value.startswith("resources/"):
        return str(resolve_repo_path(value))

    return value


def build_remote_payload_item(model_entry: dict[str, Any],
                              input_spec: dict[str, Any],
                              endpoint: str,
                              headers: dict[str, str]) -> Any:
    input_type = input_spec["type"]
    label = input_spec.get("label", "")
    override = get_input_override(model_entry, label)

    if input_type == "audio_track":
        if override is None and not input_spec.get("required", True):
            return None

        local_path = Path(override) if override is not None else AUDIO_FIXTURE_PATH
        remote_path = upload_remote_file(endpoint, local_path, headers)
        return wrap_remote_file(remote_path)

    if input_type == "midi_track":
        if override is None and not input_spec.get("required", True):
            return None

        local_path = Path(override) if override is not None else MIDI_FIXTURE_PATH
        remote_path = upload_remote_file(endpoint, local_path, headers)
        return wrap_remote_file(remote_path)

    if input_type == "slider":
        return input_spec.get("value")

    if input_type == "toggle":
        return input_spec.get("value", False)

    if input_type == "number_box":
        return input_spec.get("value", input_spec.get("minimum", 0))

    if input_type == "dropdown":
        if override is not None:
            return override

        choices = input_spec.get("choices", [])
        value = input_spec.get("value")
        return value if value is not None else normalize_dropdown_choice(choices[0])

    if input_type == "text_box":
        if override is not None:
            return override

        value = input_spec.get("value")
        return value if value not in (None, "") else "Short validation prompt"

    if input_type == "json":
        return {}

    raise ValueError(f"Unsupported remote input type: {input_type}")


def infer_expected_outputs_from_controls(controls: dict[str, Any]) -> list[dict[str, Any]]:
    expected: list[dict[str, Any]] = []

    for output_spec in controls.get("outputs", []):
        output_type = output_spec["type"]

        if output_type == "audio_track":
            expected.append({"kind": "audio_file", "extension": ".wav", "min_size_bytes": 128})
        elif output_type == "midi_track":
            expected.append({"kind": "midi_file", "extension": ".mid", "min_size_bytes": 32})
        elif output_type == "json":
            expected.append({"kind": "label_list"})
        else:
            raise ValueError(f"Unsupported remote output type: {output_type}")

    return expected


def download_remote_output(url: str) -> Path:
    parsed = urlparse(url)
    suffix = Path(parsed.path).suffix or ".bin"

    with requests.get(url, stream=True, timeout=120) as response:
        response.raise_for_status()

        with tempfile.NamedTemporaryFile(delete=False, suffix=suffix) as handle:
            for chunk in response.iter_content(chunk_size=65536):
                if chunk:
                    handle.write(chunk)

            return Path(handle.name)


def normalize_remote_outputs(result: list[Any],
                             expected_outputs: list[dict[str, Any]]) -> tuple[list[Any], list[Path]]:
    normalized: list[Any] = []
    cleanup_paths: list[Path] = []

    for output_value, expected in zip(result, expected_outputs):
        if expected["kind"] == "label_list":
            normalized.append(output_value)
            continue

        if not isinstance(output_value, dict):
            raise AssertionError(f"Expected output dictionary, got: {type(output_value)!r}")

        meta = output_value.get("meta", {})
        output_type = meta.get("_type")

        if output_type != "gradio.FileData":
            raise AssertionError(f"Expected gradio.FileData output, got: {output_type!r}")

        downloaded = download_remote_output(output_value["url"])
        cleanup_paths.append(downloaded)
        normalized.append(str(downloaded))

    return normalized, cleanup_paths


def query_remote_gradio_controls(model_entry: dict[str, Any]) -> dict[str, Any]:
    endpoint = infer_gradio_endpoint_path(model_entry["path"])
    headers = build_auth_headers(model_entry)
    event_id = post_gradio_call(endpoint, "controls", {"data": []}, headers)
    response = wait_for_gradio_event(endpoint, "controls", event_id, headers)
    return response[0]


def run_remote_gradio_validation(model_entry: dict[str, Any]) -> list[Path]:
    endpoint = infer_gradio_endpoint_path(model_entry["path"])
    headers = build_auth_headers(model_entry)
    controls = query_remote_gradio_controls(model_entry)
    payload = {
        "data": [
            build_remote_payload_item(model_entry, input_spec, endpoint, headers)
            for input_spec in controls.get("inputs", [])
        ]
    }
    event_id = post_gradio_call(endpoint, "process", payload, headers)
    result = wait_for_gradio_event(endpoint, "process", event_id, headers)
    expected_outputs = infer_expected_outputs_from_controls(controls)
    normalized_result, cleanup_paths = normalize_remote_outputs(result, expected_outputs)

    try:
        validate_outputs(normalized_result, {"validation": {"expected_outputs": expected_outputs}})
    except Exception:
        cleanup_outputs(cleanup_paths)
        raise

    return cleanup_paths


def load_stability_controls(model_entry: dict[str, Any]) -> dict[str, Any]:
    if model_entry["path"] == "stability/text-to-audio":
        path = REPO_ROOT / "src" / "clients" / "providers" / "stability" / "models" / "text-to-audio.json"
    elif model_entry["path"] == "stability/audio-to-audio":
        path = REPO_ROOT / "src" / "clients" / "providers" / "stability" / "models" / "audio-to-audio.json"
    else:
        raise ValueError(f"Unsupported Stability path: {model_entry['path']}")

    return json.loads(path.read_text(encoding="utf-8"))


def run_stability_remote_validation(model_entry: dict[str, Any]) -> list[Path]:
    token = get_required_env_value(model_entry) or get_first_env_value(STABILITY_TOKEN_ENVS)

    if not token:
        raise RuntimeError("Missing Stability API key.")

    controls = load_stability_controls(model_entry)
    inputs = controls["inputs"]
    mapped_values = {
        input_spec["label"]: build_remote_payload_item(
            model_entry,
            input_spec,
            endpoint="",
            headers={},
        )
        for input_spec in inputs
        if input_spec["type"] != "audio_track"
    }

    data = {
        "duration": str(int(mapped_values["Duration (s)"])),
        "steps": str(int(mapped_values["steps"])),
        "cfg_scale": str(int(mapped_values["cfg"])),
        "output_format": str(mapped_values["Output Format"]),
        "prompt": str(mapped_values["Text Prompt"]),
    }

    files = None

    if model_entry["path"] == "stability/audio-to-audio":
        files = {
            "audio": (
                AUDIO_FIXTURE_PATH.name,
                AUDIO_FIXTURE_PATH.open("rb"),
                "audio/wav",
            )
        }

    try:
        endpoint = "https://api.stability.ai/v2beta/audio/stable-audio-2/"
        endpoint += "audio-to-audio" if model_entry["path"].endswith("audio-to-audio") else "text-to-audio"

        response = requests.post(
            endpoint,
            headers={"Authorization": f"Bearer {token}", "Accept": "audio/*,application/json"},
            data=data,
            files=files,
            timeout=180,
        )
    finally:
        if files:
            files["audio"][1].close()

    response.raise_for_status()

    suffix = "." + data["output_format"]

    with tempfile.NamedTemporaryFile(delete=False, suffix=suffix) as handle:
        handle.write(response.content)
        output_path = Path(handle.name)

    validate_outputs(
        [str(output_path)],
        {"validation": {"expected_outputs": [{"kind": "audio_file", "extension": suffix, "min_size_bytes": 128}]}}
    )

    return [output_path]


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
