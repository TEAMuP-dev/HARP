import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))
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
                            ("muq_ranking", "Similarity Ranking")):
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


if __name__ == "__main__":
    unittest.main()
