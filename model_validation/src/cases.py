"""
Test-case handling for HARP model validation.

Covers the full input/output cycle of one /process test case:
synthesizing default inputs from a model's /controls spec, overlaying a
configured case's control values and input files, and checking the outputs.
The deeper output checks live alongside: expectations.py for the declarative
per-output `expect` rules and validators.py for the custom validators.
"""

import os
from pathlib import Path

from gradio_client import handle_file

from assets import Assets
from expectations import FILE_TYPES, resolve_expect_targets, check_expectations
from validators import VALIDATORS, ValidatorNotApplicable


__all__ = [
    'synthesize_default_args',
    'apply_case',
    'validate_outputs',
    'inspect_outputs'
]


def synthesize_default_args(controls: dict, assets: Assets,
                            synth: dict | None = None) -> tuple:
    """
    Build the positional argument list for /process from the /controls spec.

    Args:
        controls (dict): The /controls payload (card, inputs, outputs).
        assets (Assets): Synthesized input files to draw from.
        synth (dict | None): Merged `synthesized_inputs` block controlling the
            properties of the generated inputs (see config.yml).

    Returns:
        args (list): One argument per input component, in declaration order.
        missing (dict): Label -> reason for every input that could NOT be
            synthesized. Such inputs get a None placeholder. A test case can
            still run if its controls/files overrides cover them all.
    """

    args, missing = [], {}

    for spec in controls.get("inputs", []):
        ctype = spec.get("type")
        label = spec.get("label")
        required = spec.get("required", True)

        if ctype in FILE_TYPES:
            # Optional file inputs are left unsupplied, exercising the model's
            # own handling of their absence
            if not required:
                args.append(None)
                continue
            if ctype == "audio_track":
                path = assets.audio(synth)
            elif ctype == "midi_track":
                path = assets.midi(synth)
            else:
                path = assets.for_file_types(spec.get("file_types"), synth)
            if path is None:
                missing[label] = (f"cannot synthesize a {ctype} input for "
                                  f"file_types={spec.get('file_types')}. Supply "
                                  f"one via a test case's `files` entry")
            args.append(handle_file(str(path)) if path is not None else None)
        elif ctype in ("slider", "number_box"):
            # A number box can declare neither a default nor a minimum (both
            # arrive as None), so fall back to 0 rather than sending None and
            # failing inside the model with what looks like a model fault
            value = spec.get("value")
            if value is None:
                value = spec.get("minimum")
            args.append(value if value is not None else 0)
        elif ctype == "text_box":
            # An empty string is a legitimate declared default, so only fall
            # back when no default was declared at all
            value = spec.get("value")
            args.append(value if value is not None else "test")
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
            controls: {<input label>: <value>}, overriding scalar values.
            files: {<input label>: <path>}, overriding track/file inputs
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

    def index_of(label: str) -> int:
        """Locate an input by label, rejecting unknown or ambiguous names."""
        if label not in labels:
            raise ValueError(f"test case '{case.get('name')}' references unknown "
                             f"input '{label}' (available: {labels})")
        if labels.count(label) > 1:
            raise ValueError(f"test case '{case.get('name')}' references '{label}', "
                             f"but the model has {labels.count(label)} inputs with "
                             f"that label, so overriding it is ambiguous")
        return labels.index(label)

    for label, value in (case.get("controls") or {}).items():
        args[index_of(label)] = value

    for label, rel_path in (case.get("files") or {}).items():
        path = (config_dir / rel_path).resolve()
        if not path.exists():
            raise ValueError(f"test case '{case.get('name')}': file not found: {path}")
        args[index_of(label)] = handle_file(str(path))

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

        # gradio_client downloads file outputs and returns local paths, so a
        # path that is not a file on disk means the output never arrived
        path = out.get("path") if isinstance(out, dict) and "path" in out else out
        if not isinstance(path, (str, os.PathLike)):
            return (f"output '{spec.get('label')}' is not a file path: "
                    f"{str(out)[:100]}")
        if not os.path.isfile(path):
            return f"output file for '{spec.get('label')}' does not exist: {path}"
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
    Apply a test case's deeper output checks.

    These are its `expect` rules, which are declarative and per output (see
    expectations.py), and its `validators`, which are custom Python for
    checks spanning several outputs (see validators.py). Both are optional.
    Without either, outputs are still subject to validate_outputs().

    Args:
        result: The raw value returned by gradio_client for /process.
        controls (dict): The /controls payload.
        case (dict): Test case entry from config.yml.

    Raises:
        AssertionError: If an expectation or validator check fails.
        ValueError: If the case references an unknown output, rule, or
            validator, or applies a rule to an incompatible output.
    """

    specs = controls.get("outputs", [])
    out_map = outputs_by_label(result, specs)
    out_types = {spec.get("label"): spec.get("type") for spec in specs}

    for label, rules in resolve_expect_targets(case.get("expect"), out_types):
        check_expectations(label, out_types[label], out_map[label], rules)

    for name, params in (case.get("validators") or {}).items():
        if name not in VALIDATORS:
            raise ValueError(f"unknown validator '{name}' (available: "
                             f"{sorted(VALIDATORS)}). Register it in "
                             f"validators.py")
        try:
            VALIDATORS[name](out_map, controls, params or {})
        except ValidatorNotApplicable:
            # The model lacks the outputs this validator needs, so skip it. A
            # common case's validator must not fail models it does not fit.
            continue
