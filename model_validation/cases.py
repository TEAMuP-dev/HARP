"""
Test-case handling for HARP model validation.

Covers the full input/output cycle of one /process test case:
synthesizing default inputs from a model's /controls spec, overlaying a
configured case's control values and input files, and checking the outputs
(structural validation plus optional per-case `expect` rules and custom
validators).
"""

import os
from pathlib import Path

from gradio_client import handle_file

from assets import Assets
from validators import VALIDATORS


__all__ = [
    'synthesize_default_args',
    'apply_case',
    'validate_outputs',
    'inspect_outputs'
]


def synthesize_default_args(controls: dict, assets: Assets) -> tuple:
    """
    Build the positional argument list for /process from the /controls spec.

    Args:
        controls (dict): The /controls payload (card, inputs, outputs).
        assets (Assets): Synthesized input files to draw from.

    Returns:
        args (list): One argument per input component, in declaration order.
        missing (dict): Label -> reason for every input that could NOT be
            synthesized. Such inputs get a None placeholder; a test case can
            still run if its controls/files overrides cover them all.
    """

    args, missing = [], {}

    for spec in controls.get("inputs", []):
        ctype = spec.get("type")
        label = spec.get("label")

        if ctype == "audio_track":
            args.append(handle_file(str(assets.wav)) if spec.get("required", True) else None)
        elif ctype == "midi_track":
            args.append(handle_file(str(assets.midi)) if spec.get("required", True) else None)
        elif ctype == "generic_file":
            path = assets.for_file_types(spec.get("file_types"))
            if path is None and spec.get("required", True):
                missing[label] = f"cannot synthesize input for file_types={spec.get('file_types')}"
            args.append(handle_file(str(path)) if path is not None else None)
        elif ctype in ("slider", "number_box"):
            value = spec.get("value")
            if value is None:
                value = spec.get("minimum", 0)
            args.append(value)
        elif ctype == "text_box":
            args.append(spec.get("value") or "test")
        elif ctype == "toggle":
            args.append(bool(spec.get("value", False)))
        elif ctype == "dropdown":
            value = spec.get("value")
            if value is None:
                choices = spec.get("choices") or []
                if not choices:
                    missing[label] = "dropdown has no choices"
                else:
                    # Choices arrive as [label, value] pairs or plain values
                    first = choices[0]
                    value = first[1] if isinstance(first, (list, tuple)) and len(first) > 1 else first
            args.append(value)
        else:
            missing[label] = f"unsupported input control type '{ctype}'"
            args.append(None)

    return args, missing


def apply_case(args: list, controls: dict, case: dict, config_dir: Path) -> list:
    """
    Overlay a configured test case onto the default argument list.

    Args:
        args (list): Default arguments from synthesize_default_args().
        controls (dict): The /controls payload, used to match labels.
        case (dict): Test case entry from config.yml. May contain:
            controls: {<input label>: <value>} - override scalar values.
            files: {<input label>: <path>} - override track/file inputs
                (paths relative to config.yml).
        config_dir (Path): Directory containing config.yml, the base for
            relative file paths.

    Returns:
        args (list): A new argument list with the overrides applied.

    Raises:
        ValueError: If a label does not match any input, or a file is missing.
    """

    labels = [spec.get("label") for spec in controls.get("inputs", [])]
    args = list(args)

    for label, value in (case.get("controls") or {}).items():
        if label not in labels:
            raise ValueError(f"test case '{case.get('name')}' references unknown "
                             f"control '{label}' (available: {labels})")
        args[labels.index(label)] = value

    for label, rel_path in (case.get("files") or {}).items():
        if label not in labels:
            raise ValueError(f"test case '{case.get('name')}' references unknown "
                             f"input '{label}' (available: {labels})")
        path = (config_dir / rel_path).resolve()
        if not path.exists():
            raise ValueError(f"test case '{case.get('name')}': file not found: {path}")
        args[labels.index(label)] = handle_file(str(path))

    return args


def as_output_list(result, specs: list) -> list:
    """
    Normalize a /process result to one value per output spec.

    With a single output the raw value is used as-is, so a JSON output
    returning a list is not mistaken for multiple outputs.

    Args:
        result: The raw value returned by gradio_client for /process.
        specs (list): Output component specs from /controls.

    Returns:
        outputs (list): One value per output spec.
    """

    if len(specs) <= 1:
        return [result]

    return list(result) if isinstance(result, (list, tuple)) else [result]


def validate_outputs(result, controls: dict) -> str | None:
    """
    Structurally check the outputs every model must satisfy.

    File outputs must be present, on disk, and non-empty. JSON outputs carry
    optional data such as a pyharp LabelList: None/absent is valid (labels
    are optional), but a present value must be JSON-shaped and a LabelList
    must be well-formed.

    Args:
        result: The raw value returned by gradio_client for /process.
        controls (dict): The /controls payload.

    Returns:
        error (str | None): A description of the first problem found, or
            None when the outputs look structurally sound.
    """

    specs = controls.get("outputs", [])
    outputs = as_output_list(result, specs)

    if len(specs) > 1 and len(outputs) != len(specs):
        return f"expected {len(specs)} outputs, got {len(outputs)}"

    for spec, out in zip(specs, outputs):
        if spec.get("type") == "json":
            if out is None:
                continue
            if not isinstance(out, (dict, list)):
                return (f"JSON output '{spec.get('label')}' is not valid JSON "
                        f"data: {str(out)[:100]}")
            if isinstance(out, dict) and "labels" in out and \
                    not isinstance(out["labels"], list):
                return f"label list output '{spec.get('label')}' is malformed"
            continue

        if out is None:
            return f"output '{spec.get('label')}' is None"

        # gradio_client downloads file outputs and returns local paths
        path = out.get("path") if isinstance(out, dict) and "path" in out else out
        if isinstance(path, str) and os.path.sep in path and os.path.exists(path):
            if os.path.getsize(path) == 0:
                return f"output file for '{spec.get('label')}' is empty"

    return None


def outputs_by_label(result, specs: list) -> dict:
    """
    Map output labels to values for inspection by validators.

    Args:
        result: The raw value returned by gradio_client for /process.
        specs (list): Output component specs from /controls.

    Returns:
        outputs (dict): Label -> local file path (file outputs) or decoded
            object (JSON outputs).
    """

    outputs = as_output_list(result, specs)
    mapped = {}

    for spec, out in zip(specs, outputs):
        value = out.get("path") if isinstance(out, dict) and "path" in out else out
        mapped[spec.get("label")] = value

    return mapped


def inspect_outputs(result, controls: dict, case: dict) -> None:
    """
    Apply a test case's deeper output checks (see README.md).

    Two mechanisms, both optional per case:
        expect: declarative per-output rules ({ext, min_bytes}).
        validator: name of a custom function registered in
            validators.py; extra case keys parameterize it.

    Args:
        result: The raw value returned by gradio_client for /process.
        controls (dict): The /controls payload.
        case (dict): Test case entry from config.yml.

    Raises:
        AssertionError: If an expectation or validator check fails.
        ValueError: If the case references an unknown output or validator.
    """

    out_map = outputs_by_label(result, controls.get("outputs", []))

    for label, rules in (case.get("expect") or {}).items():
        if label not in out_map:
            raise ValueError(f"expect references unknown output '{label}' "
                             f"(available: {list(out_map)})")
        value = out_map[label]

        ext = rules.get("ext")
        if ext and not (isinstance(value, str) and value.lower().endswith(ext.lower())):
            raise AssertionError(f"output '{label}' is not a {ext} file: {value}")

        min_bytes = rules.get("min_bytes")
        if min_bytes is not None:
            if not (isinstance(value, str) and os.path.exists(value)):
                raise AssertionError(f"output '{label}' is not a file on disk: {value}")
            size = os.path.getsize(value)
            if size < min_bytes:
                raise AssertionError(f"output file '{label}' is {size} bytes, "
                                     f"expected at least {min_bytes}")

    name = case.get("validator")
    if name:
        if name not in VALIDATORS:
            raise ValueError(f"unknown validator '{name}' (available: "
                             f"{sorted(VALIDATORS)}); register it in "
                             f"validators.py")
        VALIDATORS[name](out_map, controls, case)
