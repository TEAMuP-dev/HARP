"""
Checks for the inputs built from a `source` recording or MIDI file.

These run locally with no network and no Hugging Face token:

    python -m unittest discover -s model_validation/tests -v
"""

from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

import numpy
import soundfile
from assets import Assets, check_synthesized_inputs


class SourceTest(unittest.TestCase):
    """A scratch directory holding the sources and the built inputs."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.dir = Path(self.temp.name)
        self.assets = Assets(self.dir / "assets", base_dir=self.dir)

        # One second of stereo at 22.05 kHz, the channels told apart by level
        left = numpy.full(22050, 0.25, dtype=numpy.float32)
        soundfile.write(str(self.dir / "stereo.wav"),
                        numpy.stack([left, 2 * left], axis=1), 22050)

    def build(self, **props):
        """Build an audio input from the stereo source with these properties."""
        path = self.assets.audio({"audio": {"source": "stereo.wav", **props}})
        return soundfile.read(str(path), always_2d=True)

    def test_a_short_source_is_repeated_to_the_duration(self):
        data, rate = self.build(duration=2.5, sample_rate=22050, num_channels=2)
        self.assertEqual(rate, 22050)
        self.assertEqual(len(data), int(2.5 * 22050))

    def test_a_long_source_is_cut_to_the_duration(self):
        data, _ = self.build(duration=0.5, sample_rate=22050, num_channels=2)
        self.assertEqual(len(data), int(0.5 * 22050))

    def test_the_source_is_resampled(self):
        data, rate = self.build(duration=1.0, sample_rate=44100, num_channels=2)
        self.assertEqual(rate, 44100)
        self.assertEqual(len(data), 44100)

    def test_stereo_keeps_its_channels(self):
        data, _ = self.build(duration=1.0, sample_rate=22050, num_channels=2)
        self.assertAlmostEqual(float(data[100, 0]), 0.25, places=3)
        self.assertAlmostEqual(float(data[100, 1]), 0.5, places=3)

    def test_mono_averages_the_channels(self):
        data, _ = self.build(duration=1.0, sample_rate=22050, num_channels=1)
        self.assertEqual(data.shape[1], 1)
        self.assertAlmostEqual(float(data[100, 0]), 0.375, places=3)

    def test_a_midi_source_is_passed_as_it_is(self):
        midi = self.dir / "song.mid"
        midi.write_bytes(b"MThd\x00\x00\x00\x06\x00\x00\x00\x01\x01\xe0")
        path = self.assets.midi({"midi": {"source": "song.mid", "num_notes": 9}})
        self.assertEqual(path.read_bytes(), midi.read_bytes())


class SourceConfigTest(unittest.TestCase):
    """A `source` is checked when the configuration is, before any model runs."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.dir = Path(self.temp.name)

    def check(self, kind, source):
        check_synthesized_inputs({"synthesized_inputs": {kind: {"source": source}}},
                                 self.dir)

    def test_a_missing_source_is_rejected(self):
        with self.assertRaises(ValueError) as caught:
            self.check("audio", "absent.wav")
        self.assertIn("absent.wav", str(caught.exception))

    def test_a_source_that_is_not_audio_is_rejected(self):
        (self.dir / "notes.txt").write_text("not audio")
        with self.assertRaises(ValueError):
            self.check("audio", "notes.txt")

    def test_a_midi_source_needs_a_midi_extension(self):
        (self.dir / "song.wav").write_bytes(b"RIFF")
        with self.assertRaises(ValueError):
            self.check("midi", "song.wav")

    def test_the_repository_sources_are_accepted(self):
        test_data = Path(__file__).resolve().parents[1]
        check_synthesized_inputs(
            {"synthesized_inputs": {"audio": {"source": "test_data/test_piano_20s.wav"}}},
            test_data)


if __name__ == "__main__":
    unittest.main()
