"""
Test-case handling for HARP model validation.

Covers the full input/output cycle of one /process test case:
synthesizing default inputs from a model's /controls spec, overlaying a
configured case's control values and input files, and checking the outputs
(structural validation plus optional per-case `expect` rules and custom
validators).
"""

import inspect
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
    'inputs_by_label',
    'EXPECT_RULES'
]


# Output component types that produce a file on disk
FILE_TYPES = {"audio_track", "midi_track", "generic_file"}

# The declarative `expect` vocabulary: rule name -> (applicable output types,
# what it asserts). Anything expressible as "read a property of one output and
# compare it" belongs here rather than in a custom validator (validators.py).
# Applying a rule to an output type it does not cover is a configuration
# error, not a model failure.
EXPECT_RULES = {
    "ext": (FILE_TYPES,
            "file extension, as a string or list of accepted extensions"),
    "min_bytes": (FILE_TYPES,
                  "minimum file size in bytes"),
    "max_bytes": (FILE_TYPES,
                  "maximum file size in bytes, for catching runaway output"),
    "min_duration": ({"audio_track", "midi_track"},
                     "minimum length, in seconds"),
    "max_duration": ({"audio_track", "midi_track"},
                     "maximum length, in seconds"),
    "channels": ({"audio_track"},
                 "channel count (1 = mono, 2 = stereo); a list accepts any "
                 "of several counts"),
    "sample_rate": ({"audio_track"},
                    "sample rate in Hz; a list accepts any of several rates"),
    "min_sample_rate": ({"audio_track"},
                        "minimum sample rate in Hz, for asserting a model "
                        "does not silently downsample"),
    "bit_depth": ({"audio_track"},
                  "exact PCM bit depth, e.g. 16 or 24 (not valid for "
                  "compressed formats such as MP3 or OGG)"),
    "min_rms_db": ({"audio_track"},
                   "minimum RMS level, in dBFS (0 = full scale); -60 is the "
                   "usual threshold for 'this output is not silent'"),
    "max_rms_db": ({"audio_track"},
                   "maximum RMS level, in dBFS; catches output driven to "
                   "noise or full scale"),
    "max_peak_db": ({"audio_track"},
                    "maximum peak sample level, in dBFS; -0.1 is the usual "
                    "threshold for 'this output is not clipped'"),
    "min_notes": ({"midi_track"},
                  "minimum number of note-on events"),
    "max_notes": ({"midi_track"},
                  "maximum number of note-on events"),
    "min_labels": ({"json"},
                   "minimum number of labels in a pyharp LabelList; 0 asserts "
                   "a well-formed LabelList that may legitimately be empty"),
    "max_labels": ({"json"},
                   "maximum number of labels in a pyharp LabelList"),
    "min_length": ({"text_box"},
                   "minimum number of characters in a text output; 1 asserts "
                   "'this model returned something'"),
    "contains": ({"text_box"},
                 "substring, or list of substrings, that must all appear in "
                 "a text output (compared case-insensitively)"),
}

# Rule groups, by what they need decoded. Duration rules are shared: they read
# from whichever of audio/MIDI props matches the output's type.
DURATION_RULES = {"min_duration", "max_duration"}
AUDIO_PROP_RULES = {"channels", "sample_rate", "min_sample_rate", "bit_depth",
                    "min_rms_db", "max_rms_db", "max_peak_db"}
MIDI_PROP_RULES = {"min_notes", "max_notes"}
TEXT_RULES = {"min_length", "contains"}

# Key selecting every compatible output rather than one named output
ALL_OUTPUTS = "*"


def as_list(value) -> list:
    """
    Normalize a rule value that may be a single item or a list of them.

    Args:
        value: A scalar (str, int, float) or an iterable of them.

    Returns:
        values (list): The value(s) as a list.
    """

    return [value] if isinstance(value, (str, int, float)) else list(value)


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
                missing[label] = (f"cannot synthesize input for "
                                  f"file_types={spec.get('file_types')}; supply "
                                  f"one via a test case's `files` entry")
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


def inputs_by_label(args: list, specs: list) -> dict:
    """
    Map input labels to the values actually sent to /process.

    The mirror of outputs_by_label(), letting a validator compare what came
    back against what went in - e.g. "the returned MIDI has more notes than
    the MIDI I supplied". File inputs are unwrapped from the dict handle_file
    produces, so they read as plain local paths just like file outputs do.

    Args:
        args (list): Positional arguments passed to /process, in the order of
            the input specs.
        specs (list): Input component specs from /controls.

    Returns:
        inputs (dict): Label -> local file path (file inputs) or scalar value
            (sliders, dropdowns, text boxes, toggles).
    """

    mapped = {}

    for spec, arg in zip(specs, args):
        value = arg.get("path") if isinstance(arg, dict) and "path" in arg else arg
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
                applicable, _ = EXPECT_RULES[rule]
                for label, otype in out_types.items():
                    if otype in applicable:
                        per_label.setdefault(label, {})[rule] = value
            targets.extend(per_label.items())
            continue

        if key not in out_types:
            raise ValueError(f"expect references unknown output '{key}' "
                             f"(available: {list(out_types)})")

        for rule in rules:
            applicable, _ = EXPECT_RULES[rule]
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
        allowed = as_list(rules["ext"])
        assert isinstance(value, str) and \
            any(value.lower().endswith(str(e).lower()) for e in allowed), \
            f"output '{label}' is not a {' or '.join(map(str, allowed))} file: {value}"

    if set(rules) & {"min_bytes", "max_bytes"}:
        size = os.path.getsize(require_file(label, value))

        if "min_bytes" in rules:
            assert size >= rules["min_bytes"], \
                (f"output file '{label}' is {size} bytes, expected at least "
                 f"{rules['min_bytes']}")

        if "max_bytes" in rules:
            assert size <= rules["max_bytes"], \
                (f"output file '{label}' is {size} bytes, expected at most "
                 f"{rules['max_bytes']}")

    # --- Audio outputs -------------------------------------------------------
    if otype == "audio_track" and set(rules) & (AUDIO_PROP_RULES | DURATION_RULES):
        props = read_audio_props(label, require_file(label, value))

        if "channels" in rules:
            allowed = as_list(rules["channels"])
            assert props["channels"] in allowed, \
                (f"output '{label}': expected "
                 f"{' or '.join(map(str, allowed))} channel(s), "
                 f"got {props['channels']}")

        if "sample_rate" in rules:
            allowed = as_list(rules["sample_rate"])
            assert props["sample_rate"] in allowed, \
                (f"output '{label}': expected "
                 f"{' or '.join(map(str, allowed))} Hz, "
                 f"got {props['sample_rate']} Hz")

        if "min_sample_rate" in rules:
            assert props["sample_rate"] >= rules["min_sample_rate"], \
                (f"output '{label}' is {props['sample_rate']} Hz, expected at "
                 f"least {rules['min_sample_rate']} Hz")

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

        if "max_rms_db" in rules:
            assert props["rms_db"] <= rules["max_rms_db"], \
                (f"output '{label}' is too hot (RMS {props['rms_db']:.1f} dBFS "
                 f"> {rules['max_rms_db']} dBFS)")

        if "max_peak_db" in rules:
            assert props["peak_db"] <= rules["max_peak_db"], \
                (f"output '{label}' appears clipped (peak "
                 f"{props['peak_db']:.2f} dBFS > {rules['max_peak_db']} dBFS)")

        check_duration(label, props["duration"], rules)

    # --- MIDI outputs --------------------------------------------------------
    if otype == "midi_track" and set(rules) & (MIDI_PROP_RULES | DURATION_RULES):
        props = read_midi_props(label, require_file(label, value))

        if "min_notes" in rules:
            assert props["num_notes"] >= rules["min_notes"], \
                (f"output '{label}' has {props['num_notes']} note(s), expected "
                 f"at least {rules['min_notes']}")

        if "max_notes" in rules:
            assert props["num_notes"] <= rules["max_notes"], \
                (f"output '{label}' has {props['num_notes']} note(s), expected "
                 f"at most {rules['max_notes']}")

        check_duration(label, props["duration"], rules)

    # --- JSON / LabelList outputs --------------------------------------------
    if set(rules) & {"min_labels", "max_labels"}:
        labels = value.get("labels") if isinstance(value, dict) else None
        assert isinstance(labels, list), \
            f"output '{label}' does not contain a pyharp LabelList: {value}"

        if "min_labels" in rules:
            assert len(labels) >= rules["min_labels"], \
                (f"output '{label}' has {len(labels)} label(s), expected at "
                 f"least {rules['min_labels']}")

        if "max_labels" in rules:
            assert len(labels) <= rules["max_labels"], \
                (f"output '{label}' has {len(labels)} label(s), expected at "
                 f"most {rules['max_labels']}")

    # --- Text outputs --------------------------------------------------------
    if otype == "text_box" and set(rules) & TEXT_RULES:
        assert isinstance(value, str), \
            f"output '{label}' is not text: {type(value).__name__}"

        if "min_length" in rules:
            assert len(value.strip()) >= rules["min_length"], \
                (f"output '{label}' is {len(value.strip())} character(s), "
                 f"expected at least {rules['min_length']}")

        if "contains" in rules:
            haystack = value.lower()
            for needle in as_list(rules["contains"]):
                assert str(needle).lower() in haystack, \
                    (f"output '{label}' does not contain {str(needle)!r}: "
                     f"{value[:120]!r}")


def inspect_outputs(result, controls: dict, case: dict, args: list = None) -> None:
    """
    Apply a test case's deeper output checks (see README.md).

    Both mechanisms are optional per case; without either, an output is still
    subject to the structural checks in validate_outputs().

        expect: declarative per-output rules, the common path - see
            EXPECT_RULES for the vocabulary.
        validators: mapping of validator name -> parameters, for checks that
            cannot be expressed declaratively (e.g. spanning several
            outputs); registered in validators.py.

    Args:
        result: The raw value returned by gradio_client for /process.
        controls (dict): The /controls payload.
        case (dict): Test case entry from config.yml.
        args (list): The positional arguments sent to /process. Supplying
            them lets validators compare output against input; without them,
            a validator that asks for `inputs` receives an empty mapping.

    Raises:
        AssertionError: If an expectation or validator check fails.
        ValueError: If the case references an unknown output, rule, or
            validator, or applies a rule to an incompatible output.
    """

    specs = controls.get("outputs", [])
    out_map = outputs_by_label(result, specs)
    out_types = {spec.get("label"): spec.get("type") for spec in specs}
    in_map = inputs_by_label(args or [], controls.get("inputs", []))

    for label, rules in resolve_expect_targets(case.get("expect"), out_types):
        check_expectations(label, out_types[label], out_map[label], rules)

    for name, params in (case.get("validators") or {}).items():
        if name not in VALIDATORS:
            raise ValueError(f"unknown validator '{name}' (available: "
                             f"{sorted(VALIDATORS)}); register it in "
                             f"validators.py")
        fn = VALIDATORS[name]
        # A validator opts into seeing the inputs by declaring an `inputs`
        # parameter, so validators that only inspect outputs keep the shorter
        # three-argument signature.
        extra = ({"inputs": in_map}
                 if "inputs" in inspect.signature(fn).parameters else {})
        try:
            fn(out_map, controls, params or {}, **extra)
        except ValidatorNotApplicable:
            # The model lacks the outputs this validator needs; skip it so a
            # common case's validator does not fail on models it does not fit
            continue
