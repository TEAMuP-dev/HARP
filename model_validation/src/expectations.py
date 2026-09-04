"""
The declarative `expect` rules for HARP model validation.

A test case's `expect` block names per-output assertions, anything
expressible as "read a property of one output and compare it". This module
defines that vocabulary (EXPECT_RULES), resolves a block against a model's
outputs, and applies the rules. Checks spanning several outputs are custom
validators (validators.py) instead. config.yml documents what each rule
asserts.
"""

import os

from audio import read_audio_props
from midi import read_midi_props


__all__ = [
    'EXPECT_RULES',
    'FILE_TYPES',
    'resolve_expect_targets',
    'check_expectations'
]


# Component types that carry a file on disk
FILE_TYPES = {"audio_track", "midi_track", "generic_file"}
AUDIO, MIDI, JSON = {"audio_track"}, {"midi_track"}, {"json"}

# The `expect` vocabulary: rule name -> output types it applies to. Applying
# a rule to an output type it does not cover fails the case with a message
# naming the configuration mistake, rather than reading as a fault in the
# model.
EXPECT_RULES = {
    "ext": FILE_TYPES,
    "min_bytes": FILE_TYPES,
    "min_duration": AUDIO | MIDI,
    "max_duration": AUDIO | MIDI,
    "num_channels": AUDIO,
    "sample_rate": AUDIO,
    "bit_depth": AUDIO,
    "min_rms_db": AUDIO,
    "min_notes": MIDI,
    "note_instruments": MIDI,
    "note_channels": MIDI,
    "min_labels": JSON,
}

# The note_* rules, mapped to the decoded property each reads. Both name the
# notes rather than the file, since a MIDI file carries no single instrument
# or channel of its own, only notes that each use one.
NOTE_VALUE_RULES = {"note_instruments": "instruments",
                    "note_channels": "channels"}

# Rule groups, by what they need decoded. Duration rules are shared: they read
# from whichever of audio/MIDI props matches the output's type.
DURATION_RULES = {"min_duration", "max_duration"}
DECODED_RULES = {
    "audio_track": {"num_channels", "sample_rate", "bit_depth",
                    "min_rms_db"} | DURATION_RULES,
    "midi_track": {"min_notes"} | set(NOTE_VALUE_RULES) | DURATION_RULES,
}

# Key selecting every compatible output rather than one named output
ALL_OUTPUTS = "*"


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
    outputs). A "*" rule that matches no output is dropped, so a
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
                             f"'{key}'. Supported rules: {sorted(EXPECT_RULES)}")

        if key == ALL_OUTPUTS:
            # Fan each rule out to the outputs whose type it covers. A rule
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
                    f"of type '{out_types[key]}'. It applies to "
                    f"{sorted(applicable)} outputs")

        targets.append((key, rules))

    return targets


def check_note_values(label: str, rule: str, found: list, allowed) -> None:
    """
    Assert the notes use only the instruments or channels a rule permits.

    The check is one of coverage rather than presence: every value the notes
    carry has to be one the rule lists, and a rule can list values the file
    does not use. A file with no notes therefore satisfies it, since
    requiring notes is what `min_notes` is for.

    Args:
        label (str): Output label the rule applies to.
        rule (str): The rule name, a key of NOTE_VALUE_RULES.
        found (list): The values the notes actually use.
        allowed: The permitted value, or a list of them.

    Raises:
        AssertionError: If the notes use a value the rule does not list.
    """

    noun = rule.removeprefix("note_")
    allowed = [allowed] if isinstance(allowed, int) else list(allowed)
    unexpected = sorted(set(found) - set(allowed))

    assert not unexpected, \
        (f"output '{label}' has notes using {noun} {unexpected}, expected "
         f"only {sorted(allowed)}")


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
    # Decode once, and only when a rule needs the decoded properties
    props = None
    if set(rules) & DECODED_RULES.get(otype, set()):
        reader = read_audio_props if otype == "audio_track" else read_midi_props
        props = reader(label, require_file(label, value))

    if props is not None and otype == "audio_track":
        if "num_channels" in rules:
            assert props["num_channels"] == rules["num_channels"], \
                (f"output '{label}': expected {rules['num_channels']} "
                 f"channel(s), got {props['num_channels']}")

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

        for rule, prop in NOTE_VALUE_RULES.items():
            if rule in rules:
                check_note_values(label, rule, props[prop], rules[rule])

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
