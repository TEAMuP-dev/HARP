"""
Custom output validators for HARP model validation.

A validator is a function that inspects the outputs of one /process test case
and raises AssertionError (with a helpful message) when something is wrong.
Register one with the @validator decorator and reference it from a test case
in config.yml:

    overrides:
      teamup-tech/pitch_shifter:
        test_cases:
          - name: shift-up-octave
            controls:
              "Pitch Shift (semitones)": 12
            validator: wav_not_silent

Each validator receives:
    outputs (dict): output label -> value. File outputs (audio, MIDI, ...)
        are local filesystem paths downloaded by gradio_client; JSON outputs
        (e.g. pyharp LabelList) are the decoded objects (dict/list) or None.
    controls (dict): the full /controls payload (card, inputs, outputs).
    case (dict): the test case entry from config.yml, so a validator can
        read its own parameters (e.g. thresholds) from extra keys.
"""

import math
import struct
import wave


__all__ = [
    'VALIDATORS',
    'validator'
]


VALIDATORS = {}


def validator(name):
    """
    Register a validator function under a config-referenceable name.

    Args:
        name (str): The name test cases use in their `validator` entry.

    Returns:
        register (callable): Decorator that records the function.
    """

    def register(fn):
        VALIDATORS[name] = fn
        return fn
    return register


@validator("wav_not_silent")
def wav_not_silent(outputs, controls, case):
    """
    Assert every 16-bit WAV output contains an audible (non-silent) signal.

    The signal level is measured as RMS in dBFS (0 dBFS = full scale), a
    standard, bit-depth-independent unit that is easy to set thresholds in:
    digital silence is -inf, the noise floor of quiet recordings sits around
    -60 dBFS, and typical program material is above -40 dBFS. RMS is also
    robust where a peak measurement is not - a single stray click cannot
    make an otherwise-silent file pass.

    Optional case key `min_rms_db` (default -60.0) sets the quietest
    acceptable RMS level in dBFS.

    Raises:
        AssertionError: If a WAV output is empty/silent, or none exist.
    """
    min_rms_db = case.get("min_rms_db", -60.0)
    checked = 0
    for label, value in outputs.items():
        if not (isinstance(value, str) and value.lower().endswith(".wav")):
            continue
        with wave.open(value, "rb") as f:
            assert f.getsampwidth() == 2, \
                f"'{label}': expected 16-bit audio, got {8 * f.getsampwidth()}-bit"
            frames = f.readframes(f.getnframes())
        assert frames, f"'{label}' contains no audio frames"
        samples = struct.unpack(f"<{len(frames) // 2}h", frames)
        rms = math.sqrt(sum(s * s for s in samples) / len(samples)) / 32768.0
        rms_db = 20 * math.log10(rms) if rms > 0 else float("-inf")
        assert rms_db >= min_rms_db, \
            f"'{label}' appears silent (RMS {rms_db:.1f} dBFS < {min_rms_db} dBFS)"
        checked += 1
    assert checked > 0, "no WAV outputs found to check"


@validator("wav_format")
def wav_format(outputs, controls, case):
    """
    Assert every WAV output matches the supplied format constraints.

    Optional case keys (all optional; only the supplied ones are checked):
        channels (int): expected channel count (1 = mono, 2 = stereo).
        sample_rate (int): expected sample rate in Hz.
        min_duration (float): minimum length in seconds.

    Raises:
        AssertionError: If a WAV output fails a supplied check, or none exist.
    """
    channels = case.get("channels")
    sample_rate = case.get("sample_rate")
    min_duration = case.get("min_duration")
    checked = 0
    for label, value in outputs.items():
        if not (isinstance(value, str) and value.lower().endswith(".wav")):
            continue
        with wave.open(value, "rb") as f:
            if channels is not None:
                assert f.getnchannels() == channels, \
                    f"'{label}': expected {channels} channel(s), got {f.getnchannels()}"
            if sample_rate is not None:
                assert f.getframerate() == sample_rate, \
                    f"'{label}': expected {sample_rate} Hz, got {f.getframerate()} Hz"
            if min_duration is not None:
                duration = f.getnframes() / f.getframerate()
                assert duration >= min_duration, \
                    f"'{label}' is {duration:.2f}s, expected at least {min_duration}s"
        checked += 1
    assert checked > 0, "no WAV outputs found to check"


@validator("has_labels")
def has_labels(outputs, controls, case):
    """
    Assert at least one JSON output contains a non-empty pyharp LabelList.

    Optional case key `min_labels` (default 1) sets the minimum count.

    Raises:
        AssertionError: If no LabelList is present or it has too few labels.
    """
    min_labels = case.get("min_labels", 1)
    for label, value in outputs.items():
        if isinstance(value, dict) and isinstance(value.get("labels"), list):
            count = len(value["labels"])
            assert count >= min_labels, \
                f"'{label}' has {count} labels, expected at least {min_labels}"
            return
    raise AssertionError("no output contains a pyharp LabelList")
