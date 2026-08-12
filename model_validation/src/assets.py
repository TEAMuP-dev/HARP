"""
Synthesized test inputs for HARP model validation.

Every input file is generated from scratch - audio with soundfile, MIDI with
the standard library - so validation needs no binary fixtures checked into the
repository. Real-world inputs for specific models belong in test_data/ and are
referenced from a test case's `files` entry in config.yml.

Input properties (sample rate, channels, length, format, ...) are configurable
via `synthesized_inputs` - see config.yml for the schema. Each distinct set of
properties is generated once and cached for the run.
"""

import struct
import threading
from pathlib import Path

import numpy
import soundfile


__all__ = [
    'Assets',
    'AUDIO_DEFAULTS',
    'MIDI_DEFAULTS',
    'merge_specs'
]


# Configurable properties of synthesized inputs, with their default values
AUDIO_DEFAULTS = {
    "sample_rate": 44100,   # Hz
    "channels": 1,          # 1 = mono, 2 = stereo, ...
    "duration": 2.0,        # seconds
    "ext": ".wav",          # container/format to write
}
MIDI_DEFAULTS = {
    "num_notes": 2,         # ascending notes from middle C
    "note_duration": 0.5,   # seconds per note (at the default 120 BPM)
    "ext": ".mid",
}

# Extensions understood as audio; a component accepting one of these gets a
# synthesized clip in that format (written via soundfile, so only formats this
# libsndfile build supports can be produced).
AUDIO_EXTS = {".wav", ".flac", ".ogg", ".oga", ".opus", ".aiff", ".aif",
              ".aifc", ".au", ".snd", ".w64", ".caf", ".mp3"}

MIDI_EXTS = {".mid", ".midi"}

# MIDI ticks per quarter note, and the default tempo those ticks imply
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


class Assets:
    """
    Synthesized test input files, generated on demand and cached for the run.

    One instance is shared by every worker thread, so generation is guarded:
    without the lock, two models requesting the same input would write the
    same file concurrently and a model could be handed a truncated one.

    Args:
        workdir (Path): Directory the generated files are written to.
        defaults (dict | None): Global `synthesized_inputs` block from
            config.yml, overriding the built-in property defaults.
    """

    def __init__(self, workdir: Path, defaults: dict | None = None):
        self.workdir = workdir
        self.defaults = defaults or {}
        self._cache = {}
        self._lock = threading.Lock()

    @property
    def text(self) -> Path:
        """A plain-text input file."""

        return self.cached(("text",), lambda: write_bytes(
            self.workdir / "test_input.txt", b"HARP model validation\n"))

    @property
    def json(self) -> Path:
        """A minimal JSON input file."""

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

        with self._lock:
            if key in self._cache:
                path = self._cache[key]
                # None records a format this build cannot write, so it is
                # not retried; an existing file is reused as-is
                if path is None or path.exists():
                    return path

            self.workdir.mkdir(parents=True, exist_ok=True)
            try:
                self._cache[key] = generate()
            except Exception:  # noqa: BLE001 - format not writable here
                self._cache[key] = None

            return self._cache[key]

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
        channels = int(props["channels"])
        duration = float(props["duration"])
        ext = (ext or props["ext"]).lower()

        path = (self.workdir /
                f"test_input_{sample_rate}hz_{channels}ch_{duration:g}s{ext}")

        return self.cached(("audio", sample_rate, channels, duration, ext),
                           lambda: write_audio(path, duration, sample_rate, channels))

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
        ext = (ext or props["ext"]).lower()

        path = self.workdir / f"test_input_{num_notes}n_{note_duration:g}s{ext}"

        return self.cached(("midi", num_notes, note_duration, ext),
                           lambda: write_midi(path, num_notes, note_duration))

    def for_file_types(self, file_types: list,
                       overrides: dict | None = None) -> Path | None:
        """
        Pick a synthesized file whose format matches the accepted types.

        Audio components get a clip in an accepted format, preferring the
        configured `ext` when the component allows it; MIDI, JSON, and text
        inputs are served from their synthesized files. A component whose
        accepted types are all unsupported (e.g. a bespoke binary format) gets
        None - supply a real file via a test case's `files` entry.

        Args:
            file_types (list): Accepted extensions from the /controls spec;
                empty or None means any file is accepted.
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
                channels: int) -> Path:
    """
    Write a sine sweep - a valid input for any audio model.

    Args:
        path (Path): Destination path; its extension selects the format.
        duration (float): Length of the sweep in seconds.
        sample_rate (int): Sample rate in Hz.
        channels (int): Number of (identical) channels to write.

    Returns:
        path (Path): The written file, for chaining.
    """

    n = max(1, int(duration * sample_rate))
    t = numpy.arange(n) / sample_rate
    # Sweep from 220 Hz up one octave over the clip
    freq = 220.0 + 220.0 * numpy.arange(n) / n
    mono = 0.5 * numpy.sin(2 * numpy.pi * freq * t)
    soundfile.write(str(path), numpy.tile(mono[:, None], (1, channels)), sample_rate)

    return path


def write_midi(path: Path, num_notes: int, note_duration: float) -> Path:
    """
    Write a standard MIDI file (format 0) of ascending notes from middle C.

    Args:
        path (Path): Destination .mid path.
        num_notes (int): Number of notes to write.
        note_duration (float): Seconds each note sounds, at 120 BPM.

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

    events = bytearray([0x00, 0xC0, 0x00])          # program change: grand piano
    for i in range(max(0, num_notes)):
        pitch = 60 + (i * 2) % 24                   # ascending from middle C
        events += bytes([0x00, 0x90, pitch, 0x64])  # note on
        events += delta(ticks) + bytes([0x80, pitch, 0x40])
    events += bytes([0x00, 0xFF, 0x2F, 0x00])       # end of track

    header = b"MThd" + struct.pack(">IHHH", 6, 0, 1, MIDI_TICKS_PER_BEAT)
    track = b"MTrk" + struct.pack(">I", len(events)) + bytes(events)
    path.write_bytes(header + track)

    return path
