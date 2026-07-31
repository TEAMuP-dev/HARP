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
from audio import read_audio_props
from midi import read_midi_props
from validators import VALIDATORS, ValidatorNotApplicable


__all__ = [
    'synthesize_default_args',
    'apply_case',
    'validate_outputs',
    'inspect_outputs',
    'EXPECT_RULES'
]


# Output component types that produce a file on disk
FILE_TYPES = {"audio_track", "midi_track", "generic_file"}
AUDIO, MIDI, JSON = {"audio_track"}, {"midi_track"}, {"json"}

# The declarative `expect` vocabulary: rule name -> output types it applies to.
# Anything expressible as "read a property of one output and compare it"
# belongs here rather than in a custom validator (validators.py). Applying a
# rule to an output type it does not cover is a configuration error, not a
# model failure. config.yml documents what each rule asserts.
EXPECT_RULES = {
    "ext": FILE_TYPES,
    "min_bytes": FILE_TYPES,
    "min_duration": AUDIO | MIDI,
    "max_duration": AUDIO | MIDI,
    "channels": AUDIO,
    "sample_rate": AUDIO,
    "bit_depth": AUDIO,
    "min_rms_db": AUDIO,
    "min_notes": MIDI,
    "min_labels": JSON,
}

# Rule groups, by what they need decoded. Duration rules are shared: they read
# from whichever of audio/MIDI props matches the output's type.
DURATION_RULES = {"min_duration", "max_duration"}
DECODED_RULES = {
    "audio_track": {"channels", "sample_rate", "bit_depth", "min_rms_db"} | DURATION_RULES,
    "midi_track": {"min_notes"} | DURATION_RULES,
}

# Key selecting every compatible output rather than one named output
ALL_OUTPUTS = "*"


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
            synthesized. Such inputs get a None placeholder; a test case can
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
                                  f"file_types={spec.get('file_types')}; supply "
                                  f"one via a test case's `files` entry")
            args.append(handle_file(str(path)) if path is not None else None)
        elif ctype in ("slider", "number_box"):
            value = spec.get("value")
            if value is None:
                value = spec.get("minimum", 0)
            args.append(value)
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
        if isinstance(path, str) and os.path.isfile(path) and os.path.getsize(path) == 0:
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


def require_file(label: str, value) -> str:
    """
    Assert an output is a file on disk and return its path.

    Args:
        label (str): Output label, for the error message.
        value: The mapped output value.

    Returns:
        path (str): The verified filesystem path.

    Raises:
        AssertionError: If the output is not an existing file.
    """

    if not (isinstance(value, str) and os.path.exists(value)):
        raise AssertionError(f"output '{label}' is not a file on disk: {value}")

    return value


def resolve_expect_targets(expect: dict, out_types: dict) -> list:
    """
    Resolve an `expect` block into concrete (label, rules) pairs.

    Keys are output labels, or "*" to apply rules to every output the rule
    is compatible with (e.g. `min_rms_db` under "*" reaches only the audio
    outputs). A "*" rule that matches no output is simply dropped, so a
    generic case can safely target output types a given model lacks. An
    explicit label, by contrast, must exist and must accept the rule -
    otherwise it is a configuration error, not a model failure.

    Args:
        expect (dict): The case's `expect` block.
        out_types (dict): Output label -> component type from /controls.

    Returns:
        targets (list): (label, rules) pairs to check.

    Raises:
        ValueError: If a label, rule name, or rule/output pairing is invalid.
    """

    targets = []

    for key, rules in (expect or {}).items():
        rules = rules or {}

        unknown = set(rules) - set(EXPECT_RULES)
        if unknown:
            raise ValueError(f"unknown expect rule(s) {sorted(unknown)} for "
                             f"'{key}'; supported rules: {sorted(EXPECT_RULES)}")

        if key == ALL_OUTPUTS:
            # Fan each rule out to the outputs whose type it covers; a rule
            # matching nothing is dropped (lenient, so generic cases apply)
            per_label = {}
            for rule, value in rules.items():
                applicable = EXPECT_RULES[rule]
                for label, otype in out_types.items():
                    if otype in applicable:
                        per_label.setdefault(label, {})[rule] = value
            targets.extend(per_label.items())
            continue

        if key not in out_types:
            raise ValueError(f"expect references unknown output '{key}' "
                             f"(available: {list(out_types)})")

        for rule in rules:
            applicable = EXPECT_RULES[rule]
            if out_types[key] not in applicable:
                raise ValueError(
                    f"expect rule '{rule}' does not apply to output '{key}' "
                    f"of type '{out_types[key]}'; it applies to "
                    f"{sorted(applicable)} outputs")

        targets.append((key, rules))

    return targets


def check_duration(label: str, duration, rules: dict) -> None:
    """
    Apply the shared min_duration / max_duration rules to a decoded length.

    Args:
        label (str): Output label, for error messages.
        duration (float | None): Length in seconds, or None if undetermined.
        rules (dict): The output's rules (only duration keys are read).

    Raises:
        AssertionError: If a duration bound is not met, or duration is
            required but could not be determined.
    """

    if not (set(rules) & DURATION_RULES):
        return

    assert duration is not None, \
        f"output '{label}': could not determine its duration"

    if "min_duration" in rules:
        assert duration >= rules["min_duration"], \
            (f"output '{label}' is {duration:.2f}s, expected at least "
             f"{rules['min_duration']}s")

    if "max_duration" in rules:
        assert duration <= rules["max_duration"], \
            (f"output '{label}' is {duration:.2f}s, expected at most "
             f"{rules['max_duration']}s")


def check_expectations(label: str, otype: str, value, rules: dict) -> None:
    """
    Apply one output's declarative `expect` rules.

    Rule names and their applicability to this output are validated upstream
    by resolve_expect_targets(). Audio/MIDI files are decoded in a single
    pass, and only when a rule that needs the decoded properties is present.

    Args:
        label (str): Output label the rules apply to.
        otype (str): The output's component type from /controls.
        value: The mapped output value (file path or decoded JSON).
        rules (dict): Rule name -> expected value.

    Raises:
        AssertionError: If any rule is not satisfied.
    """

    # --- Any file output -----------------------------------------------------
    if "ext" in rules:
        allowed = rules["ext"]
        allowed = [allowed] if isinstance(allowed, str) else list(allowed)
        assert isinstance(value, str) and \
            any(value.lower().endswith(e.lower()) for e in allowed), \
            f"output '{label}' is not a {' or '.join(allowed)} file: {value}"

    if "min_bytes" in rules:
        size = os.path.getsize(require_file(label, value))
        assert size >= rules["min_bytes"], \
            f"output file '{label}' is {size} bytes, expected at least {rules['min_bytes']}"

    # --- Decoded audio/MIDI properties ---------------------------------------
    # Decode once, and only when a rule actually needs the decoded properties
    props = None
    if set(rules) & DECODED_RULES.get(otype, set()):
        reader = read_audio_props if otype == "audio_track" else read_midi_props
        props = reader(label, require_file(label, value))

    if props is not None and otype == "audio_track":
        if "channels" in rules:
            assert props["channels"] == rules["channels"], \
                (f"output '{label}': expected {rules['channels']} channel(s), "
                 f"got {props['channels']}")

        if "sample_rate" in rules:
            assert props["sample_rate"] == rules["sample_rate"], \
                (f"output '{label}': expected {rules['sample_rate']} Hz, "
                 f"got {props['sample_rate']} Hz")

        if "bit_depth" in rules:
            assert props["bit_depth"] is not None, \
                (f"output '{label}' is a compressed format ({props['subtype']}) "
                 f"with no PCM bit depth to check")
            assert props["bit_depth"] == rules["bit_depth"], \
                (f"output '{label}': expected {rules['bit_depth']}-bit audio, "
                 f"got {props['bit_depth']}-bit ({props['subtype']})")

        if "min_rms_db" in rules:
            assert props["rms_db"] >= rules["min_rms_db"], \
                (f"output '{label}' appears silent (RMS {props['rms_db']:.1f} dBFS "
                 f"< {rules['min_rms_db']} dBFS)")

    if props is not None and otype == "midi_track":
        if "min_notes" in rules:
            assert props["num_notes"] >= rules["min_notes"], \
                (f"output '{label}' has {props['num_notes']} note(s), expected "
                 f"at least {rules['min_notes']}")

    if props is not None:
        check_duration(label, props["duration"], rules)

    # --- JSON / LabelList outputs --------------------------------------------
    if "min_labels" in rules:
        labels = value.get("labels") if isinstance(value, dict) else None
        assert isinstance(labels, list), \
            f"output '{label}' does not contain a pyharp LabelList: {value}"
        assert len(labels) >= rules["min_labels"], \
            (f"output '{label}' has {len(labels)} label(s), expected at least "
             f"{rules['min_labels']}")


def inspect_outputs(result, controls: dict, case: dict) -> None:
    """
    Apply a test case's deeper output checks: its `expect` rules (declarative,
    per output - see EXPECT_RULES) and its `validators` (custom Python for
    checks spanning several outputs - see validators.py). Both are optional;
    without either, outputs are still subject to validate_outputs().

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
                             f"{sorted(VALIDATORS)}); register it in "
                             f"validators.py")
        try:
            VALIDATORS[name](out_map, controls, params or {})
        except ValidatorNotApplicable:
            # The model lacks the outputs this validator needs; skip it so a
            # common case's validator does not fail on models it does not fit
            continue
