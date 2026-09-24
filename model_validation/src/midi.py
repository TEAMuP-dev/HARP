"""
MIDI decoding for HARP model validation.

Parallels audio.py: reads a MIDI output and measures the handful of
properties `expect` rules check. Uses mido, a small pure-Python parser, so
it handles running status and tempo maps correctly without a native
dependency.
"""

import mido


__all__ = [
    'read_midi_props',
    'note_voices'
]


def read_midi_props(label: str, path: str) -> dict:
    """
    Parse a MIDI file and measure the properties `expect` rules check.

    Args:
        label (str): Output label, for error messages.
        path (str): Path to the MIDI file.

    Returns:
        props (dict): num_tracks, num_notes (note-on events with non-zero
            velocity), duration (seconds, or None when it cannot be
            determined, e.g. an asynchronous format-2 file), and the sorted
            instruments and channels the notes use (see note_voices).

    Raises:
        AssertionError: If the file cannot be parsed as MIDI.
    """

    try:
        midi = mido.MidiFile(path)
    except Exception as exc:  # noqa: BLE001 - any parse failure
        raise AssertionError(f"output '{label}' could not be parsed as MIDI: {exc}")

    num_notes = sum(1 for track in midi.tracks for msg in track
                    if msg.type == "note_on" and msg.velocity > 0)

    try:
        duration = midi.length
    except (ValueError, KeyError):
        # length is undefined for asynchronous (format 2) files
        duration = None

    instruments, channels = note_voices(midi)

    return {
        "num_tracks": len(midi.tracks),
        "num_notes": num_notes,
        "duration": duration,
        "instruments": instruments,
        "channels": channels,
    }


def note_voices(midi) -> tuple:
    """
    Collect the instruments and channels the notes are played with.

    A note's instrument is the program last set on its channel. Program
    changes apply only to their own channel, so each channel is tracked
    separately. A file may spread one channel's events over several tracks
    that play together, so the tracks are merged into a single time-ordered
    stream before scanning. Notes preceding any program change on their
    channel report the General MIDI default of 0.

    Args:
        midi (mido.MidiFile): The parsed file.

    Returns:
        instruments (list): Sorted program numbers the notes use, 0 to 127.
        channels (list): Sorted channels the notes use, 0 to 15.
    """

    programs = {}
    instruments, channels = set(), set()

    for msg in mido.merge_tracks(midi.tracks):
        if msg.type == "program_change":
            programs[msg.channel] = msg.program
        elif msg.type == "note_on" and msg.velocity > 0:
            instruments.add(programs.get(msg.channel, 0))
            channels.add(msg.channel)

    return sorted(instruments), sorted(channels)
