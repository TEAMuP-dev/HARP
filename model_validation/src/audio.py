"""
Audio decoding for HARP model validation.

Models return audio in whatever format their process function writes -
pyharp's save_audio() defaults to WAV, but a model passing its own output
path can produce FLAC, OGG, MP3, AIFF, and so on. All formats are decoded
through soundfile (libsndfile), which also frees the checks from caring
about bit depth: samples always arrive as floats in [-1, 1], so a level
threshold means the same thing for 16-bit, 24-bit, and float sources.
"""

import math
import os

import soundfile


__all__ = [
    'read_audio_props',
    'supported_formats'
]


def supported_formats() -> list:
    """
    List the audio formats this installation can decode.

    Returns:
        formats (list): Sorted format names (e.g. WAV, FLAC, MP3, OGG).
    """

    try:
        return sorted(soundfile.available_formats())
    except Exception:  # noqa: BLE001 - diagnostics must never raise
        return []


def bit_depth_from_subtype(subtype: str):
    """
    Map a libsndfile subtype to a PCM bit depth, when it has one.

    Args:
        subtype (str): soundfile subtype, e.g. "PCM_16", "FLOAT", "VORBIS".

    Returns:
        depth (int | None): Bits per sample for PCM and float encodings, or
            None for compressed encodings, which have no fixed bit depth.
    """

    if subtype.startswith("PCM_"):
        tail = subtype[len("PCM_"):]
        if tail in ("S8", "U8"):
            return 8
        if tail.isdigit():
            return int(tail)
    return {"FLOAT": 32, "DOUBLE": 64}.get(subtype)


def read_audio_props(label: str, path: str) -> dict:
    """
    Decode an audio file and measure the properties `expect` rules check.

    Args:
        label (str): Output label, for error messages.
        path (str): Path to the audio file.

    Returns:
        props (dict): num_channels, sample_rate, duration (seconds), rms_db
            (RMS level in dBFS, or -inf for digital silence), subtype
            (libsndfile encoding name), and bit_depth (int, or None for
            compressed encodings).

    Raises:
        AssertionError: If the file cannot be decoded or contains no audio.
    """

    try:
        # A single header-and-data open: always_2d gives a consistent
        # (frames, channels) shape, and float64 output normalizes any bit
        # depth to [-1, 1] for the level measurement
        with soundfile.SoundFile(path) as f:
            sample_rate = f.samplerate
            channels = f.channels
            subtype = f.subtype
            data = f.read(always_2d=True, dtype="float64")
    except Exception as exc:  # noqa: BLE001 - any decode failure
        ext = os.path.splitext(path)[1] or "(no extension)"
        raise AssertionError(
            f"output '{label}' could not be decoded as audio (format {ext}): "
            f"{exc}. Decodable formats: {', '.join(supported_formats())}")

    frames = data.shape[0]
    assert frames, f"output '{label}' contains no audio frames"

    rms = math.sqrt(float((data * data).sum()) / data.size)

    return {
        "num_channels": channels,
        "sample_rate": sample_rate,
        "duration": frames / sample_rate,
        "rms_db": 20 * math.log10(rms) if rms > 0 else float("-inf"),
        "subtype": subtype,
        "bit_depth": bit_depth_from_subtype(subtype),
    }
