"""
Synthesized test inputs for HARP model validation.

Every input file is generated from scratch, audio with soundfile and MIDI
with the standard library, so validation needs no binary fixtures checked into
the repository. Real-world inputs for specific models belong in test_data/ and
are referenced from a test case's `files` entry in config.yml.

Input properties (sample rate, channels, length, format, ...) are configurable
via `synthesized_inputs`, whose schema is documented in config.yml. Each
distinct set of properties is generated once and cached for the run.
"""

import struct
import tempfile
import threading
from pathlib import Path

import numpy
import soundfile

from utils import config_levels


__all__ = [
    'Assets',
    'AUDIO_DEFAULTS',
    'MIDI_DEFAULTS',
    'SYNTHESIZED_DEFAULTS',
    'merge_specs',
    'check_synthesized_inputs'
]


# Configurable properties of synthesized inputs, with their default values
AUDIO_DEFAULTS = {
    "sample_rate": 44100,   # Hz
    "num_channels": 1,      # 1 = mono, 2 = stereo, ...
    "duration": 2.0,        # seconds
    "ext": ".wav",          # container/format to write
}
MIDI_DEFAULTS = {
    "num_notes": 2,         # ascending notes from middle C
    "note_duration": 0.5,   # seconds per note (at the default 120 BPM)
    "instrument": 0,        # General MIDI program number (0 = grand piano)
    "channel": 0,           # MIDI channel (see MIDI_DRUM_CHANNEL)
    "ext": ".mid",
}

# Channels are numbered 0 to 15 throughout validation, as they are on the
# wire and in mido, rather than the 1 to 16 a DAW displays. The two namings
# are one apart, which is why the General MIDI drum channel is 9 here and is
# the same channel widely called "channel 10". HARP counts from 1 internally
# (juce::MidiMessage::getChannel), so a note written here on channel 9 is the
# one HARP reports as channel 10 and marks as drums.
MIDI_DRUM_CHANNEL = 9

# The MIDI properties written into the file as numbers, with the range each
# accepts and how to describe it. Both land in bytes the MIDI spec reserves
# for a limited range, so an out-of-range value corrupts the stream rather
# than failing on the way out, which is why they are checked as configuration.
MIDI_RANGES = {
    "instrument": (0, 127, "a General MIDI program number (0 is a grand "
                           "piano, 24 a nylon guitar)"),
    "channel": (0, 15, "a MIDI channel counted from 0"),
}

# The media kinds a `synthesized_inputs` block can configure. The text and
# JSON placeholders below are deliberately absent, as they only ever fill a
# generic file input and have nothing worth varying.
SYNTHESIZED_DEFAULTS = {"audio": AUDIO_DEFAULTS, "midi": MIDI_DEFAULTS}

# Extensions understood as audio. A component accepting one of these gets a
# synthesized clip in that format (written via soundfile, so only formats this
# libsndfile build supports can be produced).
AUDIO_EXTS = {".wav", ".flac", ".ogg", ".oga", ".opus", ".aiff", ".aif",
              ".aifc", ".au", ".snd", ".w64", ".caf", ".mp3"}

MIDI_EXTS = {".mid", ".midi"}

# MIDI time base: ticks per quarter note written into the file header, and the
# seconds per quarter note they represent. No tempo event is written, so the
# MIDI default of 120 BPM (0.5s per beat) applies when the file is read.
MIDI_TICKS_PER_BEAT = 480
MIDI_SECONDS_PER_BEAT = 0.5


def merge_specs(*specs) -> dict:
    """
    Merge `synthesized_inputs` blocks, later entries winning per property.

    Args:
        *specs (dict | None): Blocks keyed by media kind ("audio", "midi"),
            in increasing order of precedence.

    Returns:
        merged (dict): One block with per-kind properties merged.
    """

    merged = {}

    for spec in specs:
        for kind, props in (spec or {}).items():
            merged.setdefault(kind, {}).update(props or {})

    return merged


def can_write_audio(ext: str) -> bool:
    """
    Whether this libsndfile build can write audio with the given extension.

    Args:
        ext (str): File extension to test, leading dot included.

    Returns:
        writable (bool): True when a file with that extension can be written.
    """

    with tempfile.TemporaryDirectory() as probe_dir:
        try:
            soundfile.write(str(Path(probe_dir) / f"probe{ext}"),
                            numpy.zeros((1, 1)), 44100)
        except Exception:  # noqa: BLE001 - the format is not writable here
            return False

    return True


def synthesized_input_blocks(config: dict) -> list:
    """
    Collect every `synthesized_inputs` block in the configuration.

    Such a block can appear at any level of the configuration, so the levels
    are walked via utils.config_levels() and a mistake is found wherever it
    was written.

    Args:
        config (dict): Parsed configuration.

    Returns:
        blocks (list): (where, block) pairs, where `where` names the setting
            for reporting.
    """

    return [(where, block) for where, _, mapping in config_levels(config)
            if (block := (mapping or {}).get("synthesized_inputs"))]


def check_midi_number(prop: str, value, where: str) -> None:
    """
    Reject a numeric MIDI property that falls outside the range it accepts.

    Args:
        prop (str): The property being checked, a key of MIDI_RANGES.
        value: The configured value.
        where (str): Where it was written, for reporting.

    Raises:
        ValueError: If the value is not a whole number within the range.
    """

    low, high, description = MIDI_RANGES[prop]

    # bool is a subclass of int, so `channel: true` would otherwise pass as
    # channel 1 rather than being reported as the mistake it is
    if isinstance(value, bool) or not isinstance(value, int) \
            or not low <= value <= high:
        raise ValueError(
            f"synthesized_inputs midi {prop} {value!r} (set in {where}) is "
            f"not {description}. Give a whole number from {low} to {high}")


def check_synthesized_inputs(config: dict) -> None:
    """
    Reject `synthesized_inputs` settings that nothing would act on.

    Only audio and MIDI inputs are built from properties. Text and JSON inputs
    are fixed placeholder files, and any other input a model declares has to
    come from a test case's `files` entry. An unrecognized media kind or
    property name would therefore be read by nothing at all, an audio format
    this installation cannot write would leave models without inputs, and an
    out-of-range MIDI number would corrupt the file it is written into.
    Reporting them here turns each into one configuration error naming the
    setting, rather than a set of models with skipped or misleading cases.

    Args:
        config (dict): Parsed configuration.

    Raises:
        ValueError: If a setting is unrecognized or cannot be honoured.
    """

    exts = {}

    for where, block in synthesized_input_blocks(config):
        for kind, props in block.items():
            if kind not in SYNTHESIZED_DEFAULTS:
                raise ValueError(
                    f"unknown synthesized_inputs kind '{kind}' (set in "
                    f"{where}). Only {' and '.join(SYNTHESIZED_DEFAULTS)} "
                    f"inputs are generated from properties")

            unknown = sorted(set(props or {}) - set(SYNTHESIZED_DEFAULTS[kind]))
            if unknown:
                raise ValueError(
                    f"unknown synthesized_inputs {kind} propert"
                    f"{'ies' if len(unknown) > 1 else 'y'} {unknown} (set in "
                    f"{where}). Valid properties: "
                    f"{sorted(SYNTHESIZED_DEFAULTS[kind])}")

            if kind == "audio" and (props or {}).get("ext"):
                exts.setdefault(str(props["ext"]), where)

            if kind == "midi":
                for prop in MIDI_RANGES:
                    if (props or {}).get(prop) is not None:
                        check_midi_number(prop, props[prop], where)

    unwritable = {ext: where for ext, where in exts.items()
                  if not can_write_audio(ext)}

    if unwritable:
        settings = ", ".join(f"'{ext}' (set in {where})"
                             for ext, where in sorted(unwritable.items()))
        raise ValueError(f"cannot write synthesized audio as {settings}. "
                         f"This installation writes: "
                         f"{', '.join(sorted(soundfile.available_formats()))}")


class Assets:
    """
    Synthesized test input files, generated on demand and cached for the run.

    One instance is shared by every worker thread, so generation is guarded
    per cache key: without that, two models requesting the same input would
    write the same file concurrently and a model could be handed a truncated
    one. Distinct inputs generate in parallel, so one slow write does not
    stall the other workers' case setup.

    Args:
        workdir (Path): Directory the generated files are written to.
        defaults (dict | None): Global `synthesized_inputs` block from
            config.yml, overriding the built-in property defaults.
    """

    def __init__(self, workdir: Path, defaults: dict | None = None):
        self.workdir = workdir
        self.defaults = defaults or {}
        self._cache = {}
        self._key_locks = {}
        self._lock = threading.Lock()

    @property
    def text(self) -> Path:
        """Placeholder file for a generic file input accepting .txt."""

        return self.cached(("text",), lambda: write_bytes(
            self.workdir / "test_input.txt", b"HARP model validation\n"))

    @property
    def json(self) -> Path:
        """Placeholder file for a generic file input accepting .json."""

        return self.cached(("json",), lambda: write_bytes(
            self.workdir / "test_input.json", b"{}\n"))

    def cached(self, key: tuple, generate) -> Path | None:
        """
        Return the file for a cache key, generating it if it is not on disk.

        A cached path is re-checked rather than trusted: these files live
        under the run's report directory, so a cleanup elsewhere can remove
        them mid-run. Regenerating is cheap, and handing back a path to a
        deleted file would fail every remaining model with an error that
        looks like the model's fault.

        Args:
            key (tuple): Identity of the file (media kind plus properties).
            generate (callable): Zero-argument builder returning the path.

        Returns:
            path (Path | None): The generated file, or None if it could not
                be written in the requested format.
        """

        # The shared lock guards only the bookkeeping. Generation runs under
        # a per-key lock, so a worker waits only on the input it needs
        with self._lock:
            key_lock = self._key_locks.setdefault(key, threading.Lock())

        with key_lock:
            with self._lock:
                if key in self._cache:
                    path = self._cache[key]
                    # None records a format this build cannot write, so it is
                    # not retried. An existing file is reused as it stands.
                    if path is None or path.exists():
                        return path

            self.workdir.mkdir(parents=True, exist_ok=True)
            try:
                path = generate()
            except Exception:  # noqa: BLE001 - format not writable here
                path = None

            with self._lock:
                self._cache[key] = path

            return path

    def resolve(self, kind: str, overrides: dict | None = None) -> dict:
        """
        Resolve the properties for one media kind.

        Precedence is built-in defaults, then the global `synthesized_inputs`
        block, then the per-model/per-case overrides.

        Args:
            kind (str): "audio" or "midi".
            overrides (dict | None): A merged `synthesized_inputs` block.

        Returns:
            props (dict): The resolved properties for that kind.
        """

        props = dict(AUDIO_DEFAULTS if kind == "audio" else MIDI_DEFAULTS)
        props.update(self.defaults.get(kind) or {})
        props.update((overrides or {}).get(kind) or {})

        return props

    def audio(self, overrides: dict | None = None, ext: str | None = None) -> Path | None:
        """
        Get a synthesized audio clip with the resolved properties.

        Args:
            overrides (dict | None): A merged `synthesized_inputs` block.
            ext (str | None): Format to write, overriding the resolved `ext`
                (used when a component only accepts certain extensions).

        Returns:
            path (Path | None): The clip, or None when this libsndfile build
                cannot write the requested format.
        """

        props = self.resolve("audio", overrides)
        sample_rate = int(props["sample_rate"])
        num_channels = int(props["num_channels"])
        duration = float(props["duration"])
        ext = (ext or props["ext"]).lower()

        path = (self.workdir / f"test_input_{sample_rate}hz_{num_channels}ch"
                               f"_{duration:g}s{ext}")

        return self.cached(("audio", sample_rate, num_channels, duration, ext),
                           lambda: write_audio(path, duration, sample_rate,
                                               num_channels))

    def midi(self, overrides: dict | None = None, ext: str | None = None) -> Path:
        """
        Get a synthesized MIDI file with the resolved properties.

        Args:
            overrides (dict | None): A merged `synthesized_inputs` block.
            ext (str | None): Extension to write, overriding the resolved one.

        Returns:
            path (Path): The MIDI file.
        """

        props = self.resolve("midi", overrides)
        num_notes = int(props["num_notes"])
        note_duration = float(props["note_duration"])
        instrument = int(props["instrument"])
        channel = int(props["channel"])
        ext = (ext or props["ext"]).lower()

        path = (self.workdir /
                f"test_input_{num_notes}n_{note_duration:g}s"
                f"_inst{instrument}_ch{channel}{ext}")

        return self.cached(
            ("midi", num_notes, note_duration, instrument, channel, ext),
            lambda: write_midi(path, num_notes, note_duration, instrument,
                               channel))

    def for_file_types(self, file_types: list,
                       overrides: dict | None = None) -> Path | None:
        """
        Pick a synthesized file whose format matches the accepted types.

        This serves the generic file inputs, the ones a model declares with a
        list of accepted extensions rather than as an audio or MIDI track. One
        accepting audio gets a clip in an accepted format, preferring the
        configured `ext` when the component allows it. One accepting MIDI,
        .json, or .txt gets the matching synthesized file. A component whose
        accepted types are all unsupported (e.g. a bespoke binary format) gets
        None, and needs a real file supplied by a test case's `files` entry.

        Args:
            file_types (list): Accepted extensions from the /controls spec.
                Empty or None means any file is accepted.
            overrides (dict | None): A merged `synthesized_inputs` block.

        Returns:
            path (Path | None): A matching synthesized file, or None when no
                synthesized format satisfies the component.
        """

        types = {str(t).lower() for t in (file_types or [])}

        if not types or "audio" in types:
            return self.audio(overrides)

        # Try the configured format first, then any other accepted audio format
        accepted_audio = types & AUDIO_EXTS
        if accepted_audio:
            preferred = self.resolve("audio", overrides)["ext"].lower()
            order = ([preferred] if preferred in accepted_audio else []) + \
                sorted(accepted_audio - {preferred})
            for candidate in order:
                clip = self.audio(overrides, ext=candidate)
                if clip is not None:
                    return clip

        accepted_midi = types & MIDI_EXTS
        if accepted_midi:
            preferred = self.resolve("midi", overrides)["ext"].lower()
            return self.midi(overrides,
                             ext=preferred if preferred in accepted_midi
                             else sorted(accepted_midi)[0])

        if ".json" in types:
            return self.json
        if types & {".txt", ".text", "text"}:
            return self.text

        return None


def write_bytes(path: Path, data: bytes) -> Path:
    """
    Write fixed bytes to a path.

    Args:
        path (Path): Destination path.
        data (bytes): Contents to write.

    Returns:
        path (Path): The written file, for chaining.
    """

    path.write_bytes(data)

    return path


def write_audio(path: Path, duration: float, sample_rate: int,
                num_channels: int) -> Path:
    """
    Write a sine sweep, which is a valid input for any audio model.

    Args:
        path (Path): Destination path, whose extension selects the format.
        duration (float): Length of the sweep in seconds.
        sample_rate (int): Sample rate in Hz.
        num_channels (int): Number of (identical) channels to write.

    Returns:
        path (Path): The written file, for chaining.
    """

    n = max(1, int(duration * sample_rate))
    t = numpy.arange(n) / sample_rate
    # Sweep from 220 Hz up one octave over the clip
    freq = 220.0 + 220.0 * numpy.arange(n) / n
    mono = 0.5 * numpy.sin(2 * numpy.pi * freq * t)
    soundfile.write(str(path), numpy.tile(mono[:, None], (1, num_channels)),
                    sample_rate)

    return path


def write_midi(path: Path, num_notes: int, note_duration: float,
               instrument: int = 0, channel: int = 0) -> Path:
    """
    Write a standard MIDI file (format 0) of ascending notes from middle C.

    Args:
        path (Path): Destination .mid path.
        num_notes (int): Number of notes to write.
        note_duration (float): Seconds each note sounds, at 120 BPM.
        instrument (int): General MIDI program number the notes are played with
        channel (int): MIDI channel the notes are written on, 0 to 15.

    Returns:
        path (Path): The written file, for chaining.
    """

    ticks = max(1, int(round(note_duration / MIDI_SECONDS_PER_BEAT * MIDI_TICKS_PER_BEAT)))

    def delta(value):
        """Encode a delta time as a MIDI variable-length quantity."""
        out = [value & 0x7F]
        value >>= 7
        while value:
            out.insert(0, (value & 0x7F) | 0x80)
            value >>= 7
        return bytes(out)

    # The channel numbering matches the low nibble of a status byte directly
    events = bytearray([0x00, 0xC0 | channel, instrument])   # program change
    for i in range(max(0, num_notes)):
        pitch = 60 + (i * 2) % 24                            # from middle C
        events += bytes([0x00, 0x90 | channel, pitch, 0x64])  # note on
        events += delta(ticks) + bytes([0x80 | channel, pitch, 0x40])
    events += bytes([0x00, 0xFF, 0x2F, 0x00])                # end of track

    header = b"MThd" + struct.pack(">IHHH", 6, 0, 1, MIDI_TICKS_PER_BEAT)
    track = b"MTrk" + struct.pack(">I", len(events)) + bytes(events)
    path.write_bytes(header + track)

    return path
