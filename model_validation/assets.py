"""
Synthesized test inputs for HARP model validation.

Every input file is generated from scratch with the standard library, so
validation needs no binary fixtures checked into the repository. Real-world
inputs for specific models belong in test_data/ and are referenced from a
test case's `files` entry in config.yml.
"""

import math
import struct
import wave
from pathlib import Path


__all__ = [
    'Assets',
    'make_test_wav',
    'make_test_midi'
]


def make_test_wav(path: Path, duration: float = 2.0, sr: int = 44100) -> Path:
    """
    Write a short mono 16-bit sine sweep - a valid input for any audio model.

    Args:
        path (Path): Destination .wav path.
        duration (float): Length of the sweep in seconds.
        sr (int): Sample rate in Hz.

    Returns:
        path (Path): The written file, for chaining.
    """

    n = int(duration * sr)

    with wave.open(str(path), "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(sr)
        frames = bytearray()
        for i in range(n):
            t = i / sr
            # Sweep from 220 Hz up one octave over the clip
            freq = 220.0 + 440.0 * t / duration
            sample = int(0.5 * 32767 * math.sin(2 * math.pi * freq * t))
            frames += struct.pack("<h", sample)
        f.writeframes(bytes(frames))

    return path


def make_test_midi(path: Path) -> Path:
    """
    Write a minimal standard MIDI file (format 0, two quarter notes).

    Args:
        path (Path): Destination .mid path.

    Returns:
        path (Path): The written file, for chaining.
    """

    track_events = bytes([
        0x00, 0xC0, 0x00,               # program change: acoustic grand
        0x00, 0x90, 0x3C, 0x64,         # note on  C4
        0x83, 0x60, 0x80, 0x3C, 0x40,   # note off C4 after 480 ticks
        0x00, 0x90, 0x40, 0x64,         # note on  E4
        0x83, 0x60, 0x80, 0x40, 0x40,   # note off E4 after 480 ticks
        0x00, 0xFF, 0x2F, 0x00,         # end of track
    ])
    header = b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480)
    track = b"MTrk" + struct.pack(">I", len(track_events)) + track_events
    path.write_bytes(header + track)

    return path


class Assets:
    """
    Synthesized test input files, generated once and shared across all
    model validations in a run.
    """

    def __init__(self, workdir: Path):
        workdir.mkdir(parents=True, exist_ok=True)
        self.wav = make_test_wav(workdir / "test_input.wav")
        self.midi = make_test_midi(workdir / "test_input.mid")
        self.text = workdir / "test_input.txt"
        self.text.write_text("HARP model validation\n")
        self.json = workdir / "test_input.json"
        self.json.write_text("{}\n")

    def for_file_types(self, file_types: list) -> Path | None:
        """
        Pick a synthesized file whose format actually matches the accepted
        types. Only extensions we can genuinely produce are matched - e.g.
        a component accepting only {".mp3", ".flac"} gets None (we cannot
        synthesize those with the stdlib), NOT a mislabeled WAV. Supply a
        real file via a test case's `files` entry in config.yml instead.

        Args:
            file_types (list): Accepted extensions from the /controls spec;
                empty or None means any file is accepted.

        Returns:
            path (Path | None): A matching synthesized file, or None when
                no synthesized format satisfies the component.
        """

        types = {str(t).lower() for t in (file_types or [])}

        if not types or ".wav" in types or "audio" in types:
            return self.wav
        if types & {".mid", ".midi"}:
            return self.midi
        if ".json" in types:
            return self.json
        if types & {".txt", ".text", "text"}:
            return self.text

        return None
