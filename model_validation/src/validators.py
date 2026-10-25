"""
Custom output validators for HARP model validation.

Most output checks do not belong here. Anything expressible as "read a
property of one output and compare it" (extension, size, channel count,
sample rate, duration, signal level, label count) is a declarative `expect`
rule in config.yml, defined by EXPECT_RULES in expectations.py.

A validator covers the checks that cannot be written that way: relationships
spanning several outputs, or logic needing real computation. Register one
with the @validator decorator and reference it from a test case, with its
parameters nested beneath the name:

    overrides:
      teamup-tech/some_transcriber:
        test_cases:
          - name: labels-line-up
            validators:
              labels_within_tracks:
                tolerance: 0.1

Each validator receives:
    outputs (dict): output label -> value. A file output (audio, MIDI, ...) is
        a local filesystem path downloaded by gradio_client. A JSON output
        (e.g. a pyharp LabelList) is the decoded object (dict/list) or None.
    controls (dict): the full /controls payload (card, inputs, outputs).
    params (dict): the parameters nested under the name of this validator,
        keeping its settings separate from the fields of the test case.

Validators signal failure by raising AssertionError. The message should name
the output at fault and say what was wrong with it.
"""

import json
import math
from pathlib import Path

from assets import input_kind
from audio import read_audio_props
from midi import read_midi_props


__all__ = [
    'VALIDATORS',
    'validator',
    'ValidatorNotApplicable'
]


class ValidatorNotApplicable(Exception):
    """
    Raised by a validator when the current model lacks the outputs it needs.

    Treated as a skip, not a failure. Raising this (rather than asserting)
    for absent outputs lets a validator run in a common test case: it applies
    to models that have the relevant outputs and is skipped on those that do
    not, mirroring the leniency of a "*" expect rule.
    """


VALIDATORS = {}


def validator(name):
    """
    Register a validator function under a config-referenceable name.

    Args:
        name (str): The name test cases use in their `validators` entry.

    Returns:
        register (callable): Decorator that records the function.
    """

    def register(fn):
        VALIDATORS[name] = fn
        return fn
    return register


def json_file(outputs, label):
    """
    Read the JSON file output of a model.

    Every failure here is reported as an AssertionError naming the output,
    since a malformed result is the fault of the model and has to read as one. A
    missing label is the exception: the model no longer has the output this
    validator was written for, which is a configuration mistake.

    Args:
        outputs (dict): Output label -> value, as passed to a validator.
        label (str): Label of the JSON file output to read.

    Returns:
        result (dict): The decoded JSON object.

    Raises:
        AssertionError: If the output is not a readable JSON object.
        ValueError: If the model has no output with this label.
    """

    if label not in outputs:
        raise ValueError(f"validator expects an output labeled '{label}', "
                         f"which this model does not have (available: "
                         f"{sorted(outputs)})")

    path = outputs[label]
    assert isinstance(path, str), \
        f"output '{label}' is not a file path: {str(path)[:100]}"

    try:
        with Path(path).open(encoding="utf-8") as stream:
            result = json.load(stream)
    except json.JSONDecodeError as exc:
        raise AssertionError(f"output '{label}' is not valid JSON: {exc}") from exc
    except OSError as exc:
        raise AssertionError(f"output '{label}' could not be read: {exc}") from exc

    assert isinstance(result, dict), \
        f"output '{label}' must hold a JSON object, got {type(result).__name__}"

    return result


def entry(mapping, key, label):
    """
    Read a required key out of the decoded result of a model.

    A model that stops returning a field is reporting a different result than
    the one it is validated against, so the absence is asserted rather than
    left to raise a KeyError that names only the key.

    Args:
        mapping (dict): The decoded object from which to read.
        key (str): The key the result must carry.
        label (str): What the mapping describes, for the error message.

    Returns:
        value: The value stored under the key.

    Raises:
        AssertionError: If the mapping is not a dict or lacks the key.
    """

    assert isinstance(mapping, dict), \
        f"{label} must be a JSON object, got {type(mapping).__name__}"
    assert key in mapping, \
        f"{label} is missing '{key}' (found: {sorted(mapping)})"

    return mapping[key]


def finite_number(value, label):
    """
    Require a numeric prediction rather than null, a string, or NaN.

    Args:
        value: The value a model reported.
        label (str): What the value is, for the error message.

    Raises:
        AssertionError: If the value is not a finite int or float. A bool is
            rejected too, since `type` is compared rather than `isinstance`.
    """

    assert type(value) in (int, float) and math.isfinite(value), \
        f"'{label}' must be a finite number, got {value!r}"


@validator("merit_scores")
def merit_scores(outputs, controls, params):
    """
    Check the three similarity scores in MERIT's result file.

    Each is a cosine similarity, so the range is the contract of the model itself
    rather than a threshold on how similar the two inputs happen to be.

    Raises:
        AssertionError: If a score is missing, is not a finite number, or
            lies outside the [-1, 1] a cosine similarity can take.
        ValueError: If the model has no "Similarity Results" output.
    """

    scores = entry(json_file(outputs, "Similarity Results"), "scores",
                   "MERIT's result")

    for name in ("melody", "rhythm", "timbre"):
        value = entry(scores, name, "MERIT's scores")
        finite_number(value, name)
        # Floating point leaves an exact 1.0 marginally over, so the bound is
        # loosened rather than rejecting a legitimately perfect match
        assert -1.000001 <= value <= 1.000001, \
            f"MERIT {name} cosine similarity is outside [-1, 1]: {value}"


@validator("muq_ranking")
def muq_ranking(outputs, controls, params):
    """
    Check candidate count, ranks, and descending similarity scores.

    Params:
        count (int): How many candidate descriptions the case supplied, which
            is how many ranked rows the model has to return.

    Raises:
        AssertionError: If the ranking is the wrong length, is misnumbered,
            carries an empty description, or is not sorted by similarity.
        ValueError: If the model has no "Similarity Ranking" output, or the
            case did not say how many candidates it supplied.
    """

    if "count" not in params:
        raise ValueError("muq_ranking needs `count`, the number of candidate "
                         "descriptions the test case supplied")

    rows = entry(json_file(outputs, "Similarity Ranking"), "results",
                 "MuQ's result")

    assert isinstance(rows, list), \
        f"MuQ's ranking must be a list, got {type(rows).__name__}"
    assert len(rows) == params["count"], \
        (f"MuQ returned {len(rows)} ranked candidate(s), expected "
         f"{params['count']}")

    previous = math.inf

    for rank, row in enumerate(rows, start=1):
        assert entry(row, "rank", f"MuQ candidate {rank}") == rank, \
            f"MuQ candidate {rank} is ranked {entry(row, 'rank', 'it')}"

        description = entry(row, "description", f"MuQ candidate {rank}")
        assert isinstance(description, str) and description.strip(), \
            f"MuQ candidate {rank} has an empty description"

        score = entry(row, "similarity", f"MuQ candidate {rank}")
        finite_number(score, "similarity")
        assert score <= previous, \
            (f"MuQ candidate {rank} scores {score}, above the {previous} "
             f"before it, so the ranking is not sorted by similarity")
        previous = score


@validator("audio_decodes")
def audio_decodes(outputs, controls, params):
    """
    Assert every audio output decodes and carries frames.

    The structural checks every case gets already require a file that exists
    and is not empty, which a header-only WAV satisfies: inference ran, wrote
    nothing, and reported success. Decoding is what catches that, and it is
    worth a validator rather than an `expect` rule because the check applies
    to whichever outputs are audio, so it reaches every model in a common
    test case without naming a label.

    Raises:
        AssertionError: If an audio output cannot be decoded or holds no
            frames (read_audio_props asserts both).
        ValidatorNotApplicable: If the model returns no audio.
    """

    audio_labels = [spec.get("label") for spec in controls.get("outputs", [])
                    if spec.get("type") == "audio_track"]

    checked = False

    for label in audio_labels:
        value = outputs.get(label)
        if isinstance(value, str):
            read_audio_props(label, value)
            checked = True

    if not checked:
        raise ValidatorNotApplicable("no audio output to decode")


@validator("declared_format")
def declared_format(outputs, controls, params):
    """
    Assert a model that takes no audio declares the format of the audio it returns.

    docs/DEPLOYMENT.md asks a model to declare SampleRate and Channels whenever
    it is fixed to one, since nothing can infer them and a user choosing a
    model for a session reads them. A model given no audio has nothing to
    follow, so the format of what it returns is its own, and is both what the
    card has to declare and what checks the declaration. A model that is given
    audio may return the format of the input instead, as the format assertions
    require, which says nothing about the format of the model itself.

    Raises:
        AssertionError: If the card declares no sample rate or channel count,
            or declares ones the audio does not have.
        ValidatorNotApplicable: If the model takes audio or returns none.
    """

    if any(input_kind(spec) == "audio" for spec in controls.get("inputs", [])):
        raise ValidatorNotApplicable("the model takes audio, so returns its format")

    audio_labels = [spec.get("label") for spec in controls.get("outputs", [])
                    if spec.get("type") == "audio_track"
                    and isinstance(outputs.get(spec.get("label")), str)]
    if not audio_labels:
        raise ValidatorNotApplicable("no audio output")

    tags = (controls.get("card") or {}).get("tags") or []
    declared = {key: value for key, _, value in (tag.partition(":") for tag in tags)
                if key in ("sample-rate", "channels")}
    channel_names = {1: "mono", 2: "stereo"}

    for label in audio_labels:
        props = read_audio_props(label, outputs[label])
        rate = str(props["sample_rate"])
        channels = channel_names.get(props["num_channels"], str(props["num_channels"]))

        assert "sample-rate" in declared, \
            (f"'{label}' is {rate} Hz audio, made from no audio input, but the card "
             f"declares no sample rate. Add SampleRate({rate}) to its tags")
        assert declared["sample-rate"] == rate, \
            (f"'{label}' is {rate} Hz audio, but the card declares "
             f"sample-rate:{declared['sample-rate']}")
        assert "channels" in declared, \
            (f"'{label}' is {channels} audio, made from no audio input, but the card "
             f"declares no channel count. Add Channels({props['num_channels']}) to its tags")
        assert declared["channels"] == channels, \
            (f"'{label}' is {channels} audio, but the card declares "
             f"channels:{declared['channels']}")


# The output types over which HARP draws labels. A label output on its own has
# nowhere to appear.
FILE_OUTPUT_TYPES = {"audio_track", "midi_track", "generic_file"}


@validator("labels_reach_harp")
def labels_reach_harp(outputs, controls, params):
    """
    Assert every JSON output is a label list HARP can show.

    HARP reads a JSON output as a pyharp LabelList and fails the whole request
    on anything else, since it needs the `meta` key to tell the output apart,
    and it draws labels only over an audio, MIDI or file output the model also
    returns. A model returning a plain dictionary passes every other check here
    and shows an error in HARP after every request, which is what six Spaces
    once did.

    Raises:
        AssertionError: If a JSON output is not a LabelList, or the model
            returns labels with no output over which to draw them.
        ValidatorNotApplicable: If the model has no JSON output.
    """

    specs = controls.get("outputs", [])
    json_labels = [spec.get("label") for spec in specs if spec.get("type") == "json"]

    if not json_labels:
        raise ValidatorNotApplicable("no JSON output")

    assert any(spec.get("type") in FILE_OUTPUT_TYPES for spec in specs), \
        (f"labels {json_labels} have no audio, MIDI or file output to appear "
         f"over, and HARP shows labels nowhere else")

    for label in json_labels:
        value = outputs.get(label)
        meta = value.get("meta") if isinstance(value, dict) else None
        kind = meta.get("_type") if isinstance(meta, dict) else None
        assert kind == "pyharp.LabelList", \
            (f"'{label}' is not a pyharp LabelList, which HARP rejects, failing the "
             f"request: {json.dumps(value)[:200]}")
        assert isinstance(value.get("labels"), list), \
            f"'{label}' is a LabelList without a list of labels"


@validator("labels_within_tracks")
def labels_within_tracks(outputs, controls, params):
    """
    Assert every returned label falls inside the track over which HARP draws it.

    HARP draws an AudioLabel over an audio output and a MidiLabel over a MIDI
    output, so each is held to the length of a track of its kind, and a label
    of any other kind to whichever track the model returns. This check warrants
    a validator rather than an `expect` rule because it relates two different
    outputs to each other, which no per-output property comparison can express.

    Params:
        tolerance (float): seconds a label may exceed the track before it is
            treated as out of bounds (default 0.05).

    Raises:
        AssertionError: If a label lies outside its track.
        ValidatorNotApplicable: If the model has no audio or MIDI output, or no
            LabelList output, for the comparison.
    """

    tolerance = params.get("tolerance", 0.05)
    readers = {"audio_track": read_audio_props, "midi_track": read_midi_props}
    kinds = {"AudioLabel": "audio_track", "MidiLabel": "midi_track"}
    names = {"audio_track": "audio", "midi_track": "MIDI"}

    # The length of the first readable output of each kind, found from the
    # component spec rather than by file extension, since a model may return
    # any format soundfile can decode
    durations = {}
    for spec in controls.get("outputs", []):
        otype, value = spec.get("type"), outputs.get(spec.get("label"))
        if otype in readers and otype not in durations and isinstance(value, str):
            duration = readers[otype](spec.get("label"), value)["duration"]
            if duration is not None:
                durations[otype] = duration
    if not durations:
        raise ValidatorNotApplicable("no audio or MIDI output to compare labels against")

    checked = False
    for label, value in outputs.items():
        if not (isinstance(value, dict) and isinstance(value.get("labels"), list)):
            continue
        checked = True
        for entry in value["labels"]:
            track = kinds.get(entry.get("label_type"))
            track = track if track in durations else next(iter(durations))
            duration, kind = durations[track], names[track]
            start = entry.get("t") or 0.0
            end = start + (entry.get("duration") or 0.0)
            assert -tolerance <= start <= duration + tolerance, \
                (f"'{label}': label \"{entry.get('label')}\" starts at {start:.2f}s, "
                 f"outside the {duration:.2f}s {kind} output")
            assert end <= duration + tolerance, \
                (f"'{label}': label \"{entry.get('label')}\" ends at {end:.2f}s, "
                 f"past the {duration:.2f}s {kind} output")

    if not checked:
        raise ValidatorNotApplicable("no pyharp LabelList output to check")
