"""Model output contracts, including valid empty predictions."""

import json
from pathlib import Path
import sys
import tempfile
import unittest

import mido

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))
from expectations import check_expectations
from validators import VALIDATORS


class ModelOutputsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "result.json"

    def check_file(self, name, label, data, params=None):
        self.path.write_text(json.dumps(data), encoding="utf-8")
        VALIDATORS[name]({label: str(self.path)}, {}, params or {})

    def test_merit_scores(self):
        self.check_file("merit_scores", "Similarity Results",
                        {"scores": {"melody": 1.0, "rhythm": 0.9, "timbre": -0.2}})
        with self.assertRaises(KeyError):
            self.check_file("merit_scores", "Similarity Results", {"scores": {}})
        for value in (None, True, "0.9", float("nan"), 2.0):
            with self.subTest(value=value), self.assertRaises(AssertionError):
                self.check_file("merit_scores", "Similarity Results",
                                {"scores": {"melody": value}})

    def test_invalid_json_is_rejected(self):
        self.path.write_text("not json" * 30)
        for name, label in (("merit_scores", "Similarity Results"),
                            ("muq_ranking", "Similarity Ranking"),
                            ("music2emo_analysis", "Emotion Analysis")):
            with self.subTest(name=name), self.assertRaises(json.JSONDecodeError):
                VALIDATORS[name]({label: str(self.path)}, {}, {})

    def test_muq_ranking(self):
        rows = [{"rank": 1, "description": "piano", "similarity": 0.8},
                {"rank": 2, "description": "drums", "similarity": 0.2}]
        self.check_file("muq_ranking", "Similarity Ranking", {"results": rows}, {"count": 2})
        with self.assertRaises(AssertionError):
            self.check_file("muq_ranking", "Similarity Ranking", {"results": rows}, {"count": 3})
        rows[1]["similarity"] = 0.9
        with self.assertRaises(AssertionError):
            self.check_file("muq_ranking", "Similarity Ranking", {"results": rows}, {"count": 2})

    def test_music2emo_allows_no_moods(self):
        result = {"valence": 4.0, "arousal": 5.0, "threshold": 0.5, "moods": []}
        self.check_file("music2emo_analysis", "Emotion Analysis", result, {"threshold": 0.5})
        result["moods"] = [{"label": "calm", "probability": 0.6}]
        self.check_file("music2emo_analysis", "Emotion Analysis", result, {"threshold": 0.5})
        result["moods"][0]["probability"] = 0.4
        with self.assertRaises(AssertionError):
            self.check_file("music2emo_analysis", "Emotion Analysis", result, {"threshold": 0.5})

    def test_required_json_outputs(self):
        for name, label, params in (("aesthetics_scores", "Aesthetic Scores", {}),
                                    ("chord_sequence", "Chord Sequence", {})):
            for value in (None, []):
                with self.subTest(name=name, value=value), self.assertRaises(AssertionError):
                    VALIDATORS[name]({label: value}, {}, params)
            with self.subTest(name=name), self.assertRaises(KeyError):
                VALIDATORS[name]({label: {}}, {}, params)

    def test_aesthetics_scores(self):
        scores = dict.fromkeys(("content_enjoyment", "content_usefulness",
                               "production_complexity", "production_quality"), 5.0)
        VALIDATORS["aesthetics_scores"]({"Aesthetic Scores": {"scores": scores}}, {}, {})
        scores["production_quality"] = float("inf")
        with self.assertRaises(AssertionError):
            VALIDATORS["aesthetics_scores"]({"Aesthetic Scores": {"scores": scores}}, {}, {})

    def test_chords_allow_empty_but_validate_timestamps(self):
        check = VALIDATORS["chord_sequence"]
        check({"Chord Sequence": {"chords": []}}, {}, {})
        rows = [{"timestamp_seconds": 0, "chord": "N"}, {"timestamp_seconds": 2, "chord": "C"}]
        check({"Chord Sequence": {"chords": rows}}, {}, {})
        rows.append({"timestamp_seconds": 4.365351, "chord": "N"})
        check({"Chord Sequence": {"chords": rows}}, {}, {})
        for timestamp in (-1, 1, float("nan")):
            rows[0]["timestamp_seconds"] = 2
            rows[1]["timestamp_seconds"] = timestamp
            with self.subTest(timestamp=timestamp), self.assertRaises(AssertionError):
                check({"Chord Sequence": {"chords": rows}}, {}, {})

    def test_game_midi_parses_without_requiring_notes(self):
        path = Path(self.temp.name) / "result.mid"
        midi = mido.MidiFile()
        midi.tracks.append(mido.MidiTrack())
        midi.save(path)
        check_expectations("Transcribed MIDI", "midi_track", str(path), {"min_notes": 0})
        path.write_bytes(b"not a midi file" * 10)
        with self.assertRaises(AssertionError):
            check_expectations("Transcribed MIDI", "midi_track", str(path), {"min_notes": 0})

    def test_deepafx_st_params(self):
        self.check_file("deepafx_st_params", "DSP Parameters",
                        {"raw_parameters": [0.5, 0.3, -0.1, 0.8]})
        with self.assertRaises(KeyError):
            self.check_file("deepafx_st_params", "DSP Parameters", {})
        for value in (None, True, "0.9", float("nan")):
            with self.subTest(value=value), self.assertRaises(AssertionError):
                self.check_file("deepafx_st_params", "DSP Parameters",
                                {"raw_parameters": [value]})


if __name__ == "__main__":
    unittest.main()
