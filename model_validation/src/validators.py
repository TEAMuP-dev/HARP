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
              labels_within_audio:
                tolerance: 0.1

Each validator receives:
    outputs (dict): output label -> value. A file output (audio, MIDI, ...) is
        a local filesystem path downloaded by gradio_client. A JSON output
        (e.g. a pyharp LabelList) is the decoded object (dict/list) or None.
    controls (dict): the full /controls payload (card, inputs, outputs).
    params (dict): the parameters nested under this validator's name, keeping
        its settings separate from the test case's own fields.

Validators signal failure by raising AssertionError. The message should name
the output at fault and say what was wrong with it.
"""

from audio import read_audio_props


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


@validator("labels_within_audio")
def labels_within_audio(outputs, controls, params):
    """
    Assert every returned label falls inside the audio output's timespan.

    This check warrants a validator rather than an `expect` rule because it
    relates two different outputs to each other, which no per-output property
    comparison can express.

    Params:
        tolerance (float): seconds a label may exceed the audio duration
            before it is treated as out of bounds (default 0.05).

    Raises:
        AssertionError: If a label lies outside the audio output.
        ValidatorNotApplicable: If the model has no audio output or no
            LabelList output for the comparison.
    """

    tolerance = params.get("tolerance", 0.05)

    # Identify the audio output from the component spec rather than by file
    # extension, since models may return any format soundfile can decode
    audio_labels = [spec.get("label") for spec in controls.get("outputs", [])
                    if spec.get("type") == "audio_track"]

    duration = None
    for label in audio_labels:
        value = outputs.get(label)
        if isinstance(value, str):
            duration = read_audio_props(label, value)["duration"]
            break
    if duration is None:
        raise ValidatorNotApplicable("no audio output to compare labels against")

    checked = False
    for label, value in outputs.items():
        if not (isinstance(value, dict) and isinstance(value.get("labels"), list)):
            continue
        checked = True
        for entry in value["labels"]:
            start = entry.get("t", 0.0)
            end = start + entry.get("duration", 0.0)
            assert -tolerance <= start <= duration + tolerance, \
                (f"'{label}': label \"{entry.get('label')}\" starts at {start:.2f}s, "
                 f"outside the {duration:.2f}s audio output")
            assert end <= duration + tolerance, \
                (f"'{label}': label \"{entry.get('label')}\" ends at {end:.2f}s, "
                 f"past the {duration:.2f}s audio output")

    if not checked:
        raise ValidatorNotApplicable("no pyharp LabelList output to check")
