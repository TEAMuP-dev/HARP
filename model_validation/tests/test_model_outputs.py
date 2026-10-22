"""
Regression checks for the custom output validators and the `expect` rules
that need more than a property comparison.

These run locally with no network and no Hugging Face token, so a change to
a validator can be checked without spending a model run on it:

    python -m unittest discover -s model_validation/tests -v
"""

import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

import soundfile
from expectations import check_expectations
from assets import check_synthesized_inputs
from harness import merge_common_cases
from utils import check_config_keys, load_config
from validators import VALIDATORS, ValidatorNotApplicable


class ValidatorTest(unittest.TestCase):
    """Shared scratch file for the validators that read a result file."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "result.json"

    def check_file(self, name, label, data, params=None):
        """Run a validator over a result file holding `data`."""
        self.path.write_text(json.dumps(data), encoding="utf-8")
        VALIDATORS[name]({label: str(self.path)}, {}, params or {})


class MeritScoresTest(ValidatorTest):
    LABEL = "Similarity Results"

    def scores(self, **overrides):
        values = {"melody": 1.0, "rhythm": 0.9, "timbre": -0.2}
        values.update(overrides)
        return {"scores": values}

    def test_accepts_three_similarities(self):
        self.check_file("merit_scores", self.LABEL, self.scores())

    def test_rejects_missing_score(self):
        # A model that stops reporting a score has to name the one it dropped
        with self.assertRaises(AssertionError) as caught:
            self.check_file("merit_scores", self.LABEL, {"scores": {}})
        self.assertIn("melody", str(caught.exception))

    def test_rejects_missing_scores_block(self):
        with self.assertRaises(AssertionError) as caught:
            self.check_file("merit_scores", self.LABEL, {"other": 1})
        self.assertIn("scores", str(caught.exception))

    def test_rejects_values_that_are_not_similarities(self):
        for value in (None, True, "0.9", float("nan"), 2.0, -2.0):
            with self.subTest(value=value), self.assertRaises(AssertionError):
                self.check_file("merit_scores", self.LABEL,
                                self.scores(melody=value))


class MuqRankingTest(ValidatorTest):
    LABEL = "Similarity Ranking"

    def rows(self):
        return [{"rank": 1, "description": "piano", "similarity": 0.8},
                {"rank": 2, "description": "drums", "similarity": 0.2}]

    def test_accepts_a_sorted_ranking(self):
        self.check_file("muq_ranking", self.LABEL,
                        {"results": self.rows()}, {"count": 2})

    def test_rejects_the_wrong_number_of_candidates(self):
        with self.assertRaises(AssertionError) as caught:
            self.check_file("muq_ranking", self.LABEL,
                            {"results": self.rows()}, {"count": 3})
        self.assertIn("expected 3", str(caught.exception))

    def test_rejects_an_unsorted_ranking(self):
        rows = self.rows()
        rows[1]["similarity"] = 0.9
        with self.assertRaises(AssertionError) as caught:
            self.check_file("muq_ranking", self.LABEL,
                            {"results": rows}, {"count": 2})
        self.assertIn("sorted", str(caught.exception))

    def test_rejects_a_misnumbered_rank(self):
        rows = self.rows()
        rows[1]["rank"] = 5
        with self.assertRaises(AssertionError):
            self.check_file("muq_ranking", self.LABEL,
                            {"results": rows}, {"count": 2})

    def test_rejects_an_empty_description(self):
        rows = self.rows()
        rows[0]["description"] = "   "
        with self.assertRaises(AssertionError):
            self.check_file("muq_ranking", self.LABEL,
                            {"results": rows}, {"count": 2})

    def test_requires_the_count_parameter(self):
        # Without it the validator would assert nothing about the length
        with self.assertRaises(ValueError):
            self.check_file("muq_ranking", self.LABEL,
                            {"results": self.rows()})


class ResultFileTest(ValidatorTest):
    """Failures in reading the file at all, which must read as model faults."""

    CASES = (("merit_scores", "Similarity Results"),
             ("muq_ranking", "Similarity Ranking"))

    def test_malformed_json_fails_the_model(self):
        self.path.write_text("not json" * 30, encoding="utf-8")
        for name, label in self.CASES:
            with self.subTest(name=name), self.assertRaises(AssertionError) as caught:
                VALIDATORS[name]({label: str(self.path)}, {}, {"count": 1})
            self.assertIn("not valid JSON", str(caught.exception))

    def test_a_json_array_is_not_a_result(self):
        for name, label in self.CASES:
            with self.subTest(name=name), self.assertRaises(AssertionError):
                self.check_file(name, label, [1, 2, 3], {"count": 1})

    def test_a_missing_output_is_a_configuration_error(self):
        # The validator names an output the model no longer has, which is the
        # config's mistake rather than a fault in the model
        for name, _ in self.CASES:
            with self.subTest(name=name), self.assertRaises(ValueError):
                VALIDATORS[name]({"Some Other Output": "x"}, {}, {"count": 1})


class AudioDecodesTest(unittest.TestCase):
    """The common case that catches an audio output holding no samples."""

    LABEL = "Output Audio"
    CONTROLS = {"outputs": [{"label": LABEL, "type": "audio_track"}]}

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = str(Path(self.temp.name) / "out.wav")

    def write(self, frames):
        soundfile.write(self.path, [0.25] * frames, 44100, subtype="PCM_16")

    def test_accepts_audio_with_frames(self):
        self.write(1024)
        VALIDATORS["audio_decodes"]({self.LABEL: self.path}, self.CONTROLS, {})

    def test_rejects_a_header_with_no_samples(self):
        # The structural checks pass this: the file exists and is not empty
        self.write(0)
        self.assertGreater(Path(self.path).stat().st_size, 0)
        with self.assertRaises(AssertionError) as caught:
            VALIDATORS["audio_decodes"]({self.LABEL: self.path}, self.CONTROLS, {})
        self.assertIn("no audio frames", str(caught.exception))

    def test_rejects_a_file_that_is_not_audio(self):
        Path(self.path).write_text("this is not a wav", encoding="utf-8")
        with self.assertRaises(AssertionError):
            VALIDATORS["audio_decodes"]({self.LABEL: self.path}, self.CONTROLS, {})

    def test_skips_a_model_with_no_audio_output(self):
        controls = {"outputs": [{"label": "Notes", "type": "midi_track"}]}
        with self.assertRaises(ValidatorNotApplicable):
            VALIDATORS["audio_decodes"]({"Notes": "x.mid"}, controls, {})


class TextExpectationsTest(unittest.TestCase):
    """The `expect` rules for a text output, which arrives as a .txt file."""

    LABEL = "Transcription"

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / "out.txt"

    def check(self, text, rules):
        self.path.write_text(text, encoding="utf-8")
        check_expectations(self.LABEL, "generic_file", str(self.path), rules)

    def test_min_length_accepts_text(self):
        self.check("C:maj G:maj", {"min_length": 1})

    def test_min_length_rejects_blank_text(self):
        # A file that exists and is non-empty can still hold only whitespace
        for text in ("", "   ", "\n"):
            with self.subTest(text=text), self.assertRaises(AssertionError):
                self.check(text, {"min_length": 1})

    def test_excludes_rejects_a_reported_failure(self):
        with self.assertRaises(AssertionError) as caught:
            self.check("Error: Could not extract notes", {"excludes": "Error:"})
        self.assertIn("Error:", str(caught.exception))

    def test_excludes_takes_a_list_and_ignores_case(self):
        with self.assertRaises(AssertionError):
            self.check("no predictions generated.",
                       {"excludes": ["Error:", "No predictions generated."]})
        self.check("C:maj G:maj", {"excludes": ["Error:"]})

    def test_a_missing_file_is_reported(self):
        with self.assertRaises(AssertionError):
            check_expectations(self.LABEL, "generic_file",
                               str(self.path) + ".absent", {"min_length": 1})

    def test_bytes_that_are_not_text_are_reported(self):
        self.path.write_bytes(b"\xff\xfe\x00binary")
        with self.assertRaises(AssertionError) as caught:
            check_expectations(self.LABEL, "generic_file", str(self.path),
                               {"min_length": 1})
        self.assertIn("could not be read as text", str(caught.exception))


class ConfigFileTest(unittest.TestCase):
    """The shipped config.yml has to stay loadable and internally consistent."""

    PATH = Path(__file__).resolve().parents[1] / "config.yml"

    def test_config_loads(self):
        self.assertIn("overrides", load_config(self.PATH))

    def test_config_settings_are_valid(self):
        # The checks a run makes before validating anything, so that a
        # misspelled setting or a source file left out of test_data/ fails here
        # rather than costing a run
        config = load_config(self.PATH)
        check_config_keys(config)
        check_synthesized_inputs(config, self.PATH.parent)

    def test_a_model_written_twice_is_rejected(self):
        # yaml keeps the last of several entries sharing a key, so a model
        # written twice would otherwise run without the first entry's
        # settings. This is what two branches configuring one model produce
        # on merge, and it is invisible in the loaded configuration
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "config.yml"
            path.write_text("overrides:\n"
                            "  teamup-tech/a:\n    process_timeout: 10\n"
                            "  teamup-tech/a:\n    process_timeout: 20\n",
                            encoding="utf-8")
            with self.assertRaises(ValueError) as caught:
                load_config(path)
            self.assertIn("duplicate key 'teamup-tech/a'", str(caught.exception))

    def test_every_validator_referenced_exists(self):
        config = load_config(self.PATH)
        cases = list(config.get("common_test_cases") or [])
        for entry in (config.get("overrides") or {}).values():
            cases += list((entry or {}).get("test_cases") or [])
        for case in cases:
            for name in (case.get("validators") or {}):
                with self.subTest(case=case.get("name"), validator=name):
                    self.assertIn(name, VALIDATORS)


class MergeCommonCasesTest(unittest.TestCase):
    """How the common cases are folded into a model's own."""

    def test_checks_are_merged_rather_than_run(self):
        # A common case without inputs costs a /process call of its own for
        # nothing, so its rules ride along with the model's own case
        cases = merge_common_cases([{"name": "decodes", "validators": {"audio_decodes": {}}}],
                                   [{"name": "default"}])
        self.assertEqual([case["name"] for case in cases], ["default"])
        self.assertIn("audio_decodes", cases[0]["validators"])

    def test_rules_on_one_output_accumulate(self):
        # Two common cases targeting the same output both keep their rules
        cases = merge_common_cases([{"name": "a", "expect": {"*": {"min_bytes": 100}}},
                                    {"name": "b", "expect": {"*": {"min_rms_db": -60}}}],
                                   [{"name": "default"}])
        self.assertEqual(cases[0]["expect"]["*"], {"min_bytes": 100, "min_rms_db": -60})

    def test_model_rules_win_on_a_conflict(self):
        cases = merge_common_cases([{"name": "a", "expect": {"*": {"min_bytes": 100}}}],
                                   [{"name": "default", "expect": {"*": {"min_bytes": 5}}}])
        self.assertEqual(cases[0]["expect"]["*"]["min_bytes"], 5)

    def test_scenarios_run_separately_with_the_model_inputs(self):
        # A common case that sends inputs is its own scenario, and inherits the
        # files the model needs but does not set itself
        cases = merge_common_cases([{"name": "stereo",
                                     "synthesized_inputs": {"audio": {"num_channels": 2}}}],
                                   [{"name": "default", "files": {"In": "a.wav"}}])
        self.assertEqual([case["name"] for case in cases], ["common:stereo", "default"])
        self.assertEqual(cases[0]["files"], {"In": "a.wav"})


if __name__ == "__main__":
    unittest.main()


class ReportTest(unittest.TestCase):
    """What the human-readable report shows for a failure."""

    def test_a_failure_carries_the_end_of_its_log(self):
        from results import ModelResult, render_markdown
        result = ModelResult(target="teamup-tech/a", error="AppError: boom",
                             log_tail=["Traceback (most recent call last):",
                                       "ValueError: the checkpoint is corrupt"])
        report = render_markdown([result], {"passed": 0, "validated": 1, "skipped": 0,
                                            "timestamp": "now", "command": ""})
        self.assertIn("ValueError: the checkpoint is corrupt", report)


class LabelsReachHarpTest(unittest.TestCase):
    """HARP reads a JSON output as a LabelList and draws it over a file output."""

    CONTROLS = {"outputs": [{"type": "audio_track", "label": "Audio"},
                            {"type": "json", "label": "Labels"}]}

    def check(self, value, controls=None):
        VALIDATORS["labels_reach_harp"]({"Labels": value}, controls or self.CONTROLS, {})

    def test_accepts_a_label_list(self):
        self.check({"meta": {"_type": "pyharp.LabelList"}, "labels": []})

    def test_rejects_a_plain_dictionary(self):
        # What six Spaces returned, which HARP fails the request on
        with self.assertRaises(AssertionError) as caught:
            self.check({"chords": [{"chord": "C"}]})
        self.assertIn("not a pyharp LabelList", str(caught.exception))

    def test_rejects_labels_with_nothing_to_appear_over(self):
        with self.assertRaises(AssertionError):
            self.check({"meta": {"_type": "pyharp.LabelList"}, "labels": []},
                       {"outputs": [{"type": "json", "label": "Labels"}]})

    def test_skips_a_model_without_a_json_output(self):
        with self.assertRaises(ValidatorNotApplicable):
            VALIDATORS["labels_reach_harp"]({}, {"outputs": [{"type": "audio_track"}]}, {})
