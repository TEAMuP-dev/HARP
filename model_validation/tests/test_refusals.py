"""
Regression checks for cases expecting an error, and for the checks added with
them: inputs to refuse, the README comparison, and the declared format.

These run locally with no network and no Hugging Face token:

    python -m unittest discover -s model_validation/tests -v
"""

from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

import gradio_client.client
import mido
import numpy
import soundfile
from assets import Assets, check_synthesized_inputs
from cases import apply_case, fed_kinds, generated_text
from harness import merge_common_cases, readme_problems
from refusals import DeclaredAppError, judge_refusal
from utils import check_config_values
from validators import VALIDATORS, ValidatorNotApplicable


def gr_error(message, title="Error"):
    """What gradio_client raises for a gr.Error, whose payload carries a title."""
    return DeclaredAppError(message=message, duration=10, visible=True, title=title)


def plain_error(message):
    """What it raises for any other exception, whose payload carries no title."""
    return DeclaredAppError(message=message, visible=True)


class JudgeRefusalTest(unittest.TestCase):

    def test_a_gr_error_saying_what_was_wrong_is_a_refusal(self):
        refused, problem, reached = judge_refusal(
            gr_error("Could not read the audio file: Format not recognised."), True)
        self.assertTrue(refused)
        self.assertEqual(problem, "")
        self.assertFalse(reached)

    def test_an_uncaught_exception_is_not(self):
        refused, problem, reached = judge_refusal(
            plain_error("zero-size array to reduction operation maximum"), True)
        self.assertFalse(refused)
        self.assertIn("uncaught exception", problem)
        self.assertFalse(reached)

    def test_an_error_inside_the_gpu_call_is_not(self):
        refused, problem, reached = judge_refusal(
            gr_error("RuntimeError", title="ZeroGPU worker error"), True)
        self.assertFalse(refused)
        self.assertTrue(reached)

    def test_a_quota_error_shows_the_request_got_past_the_checks(self):
        refused, _, reached = judge_refusal(
            gr_error("You have exceeded your GPU quota", title="ZeroGPU quota exceeded"), True)
        self.assertFalse(refused)
        self.assertTrue(reached)

    def test_a_worker_that_died_is_not(self):
        refused, _, _ = judge_refusal(
            gr_error("Processing stopped unexpectedly", title="Processing failed"), True)
        self.assertFalse(refused)

    def test_a_gr_error_carrying_only_a_type_name_is_not(self):
        refused, problem, _ = judge_refusal(gr_error("LibsndfileError"), True)
        self.assertFalse(refused)
        self.assertIn("nothing a user can act on", problem)

    def test_the_expected_text_has_to_appear(self):
        error = gr_error("The text is 5100 characters long. Kokoro reads at most 5000.")
        self.assertTrue(judge_refusal(error, "5000")[0])
        self.assertFalse(judge_refusal(error, "seconds")[0])

    def test_no_answer_in_time_is_not(self):
        refused, _, reached = judge_refusal(TimeoutError("/process did not return within 60s"),
                                            True)
        self.assertFalse(refused)
        self.assertTrue(reached)

    def test_the_client_raises_the_recording_error(self):
        self.assertIs(gradio_client.client.AppError, DeclaredAppError)
        self.assertEqual(gr_error("x").payload_fields, {"duration", "visible", "title"})


class RefusalInputsTest(unittest.TestCase):

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.assets = Assets(Path(self.temp.name))

    def test_an_unreadable_audio_file_is_not_audio(self):
        path = self.assets.audio({"audio": {"content": "unreadable", "ext": ".flac"}})
        self.assertEqual(path.suffix, ".flac")
        with self.assertRaises(Exception):
            soundfile.info(str(path))

    def test_an_empty_audio_file_is_valid_and_holds_nothing(self):
        path = self.assets.audio({"audio": {"content": "empty", "ext": ".flac",
                                            "num_channels": 2}})
        info = soundfile.info(str(path))
        self.assertEqual((info.frames, info.channels), (0, 2))

    def test_an_unreadable_midi_file_is_not_midi(self):
        path = self.assets.midi({"midi": {"content": "unreadable"}})
        with self.assertRaises(Exception):
            mido.MidiFile(str(path))

    def test_an_empty_midi_file_holds_no_notes(self):
        midi = mido.MidiFile(str(self.assets.midi({"midi": {"content": "empty"}})))
        self.assertFalse(any(msg.type == "note_on" for track in midi.tracks for msg in track))

    def test_an_unknown_content_is_a_configuration_error(self):
        config = {"synthesized_inputs": {"audio": {"content": "noise"}}}
        with self.assertRaises(ValueError):
            check_synthesized_inputs(config, Path(self.temp.name))

    def test_which_kinds_a_model_is_fed(self):
        controls = {"inputs": [
            {"type": "audio_track", "required": True},
            {"type": "midi_track", "required": False},
            {"type": "generic_file", "required": True, "file_types": [".txt"]},
            {"type": "slider"}]}
        self.assertEqual(fed_kinds(controls), {"audio"})


class GeneratedTextTest(unittest.TestCase):

    def test_text_of_an_exact_length(self):
        for length in (1, 150, 5001):
            with self.subTest(length=length):
                self.assertEqual(len(generated_text(length)), length)

    def test_a_case_asks_for_it_by_length(self):
        controls = {"inputs": [{"label": "Text", "type": "text_box"}]}
        args = apply_case(["short"], controls, {"name": "long",
                                                "controls": {"Text": {"text_length": 5001}}},
                          Path("."))
        self.assertEqual(len(args[0]), 5001)


class ConfigValuesTest(unittest.TestCase):

    def check(self, case=None, model=None, common=None):
        config = {"common_test_cases": common or [{"name": "audio-decodes"}],
                  "overrides": {"teamup-tech/x": dict(model or {},
                                                      test_cases=[case] if case else [])}}
        check_config_values(config)

    def test_a_refusal_has_no_outputs_to_check(self):
        with self.assertRaises(ValueError):
            self.check({"name": "refuse", "expect_error": True, "expect": {"*": {"min_bytes": 1}}})

    def test_expect_error_is_true_or_text(self):
        self.check({"name": "refuse", "expect_error": "5000"})
        for value in (False, "", 3):
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.check({"name": "refuse", "expect_error": value})

    def test_a_mapping_control_asks_for_text(self):
        self.check({"name": "long", "controls": {"Text": {"text_length": 10}}})
        with self.assertRaises(ValueError):
            self.check({"name": "long", "controls": {"Text": {"length": 10}}})

    def test_skipped_common_cases_have_to_exist(self):
        self.check(model={"skip_common_cases": ["audio-decodes"]})
        with self.assertRaises(ValueError):
            self.check(model={"skip_common_cases": ["no-such-case"]})


class MergeRefusalsTest(unittest.TestCase):

    def test_refusals_run_last_and_inherit_no_checks(self):
        common = [{"name": "signal", "expect": {"*": {"min_rms_db": -80}}},
                  {"name": "refuses", "synthesized_inputs": {"audio": {"content": "empty"}},
                   "expect_error": True}]
        own = [{"name": "too-long", "expect_error": "minutes"}, {"name": "default"}]
        cases = merge_common_cases(common, own)
        self.assertEqual([case["name"] for case in cases],
                         ["default", "common:refuses", "too-long"])
        self.assertNotIn("expect", cases[2])
        self.assertIn("expect", cases[0])


class ReadmeTest(unittest.TestCase):

    CARD = {"name": "Demucs", "tags": ["category:separation", "input:audio", "output:audio"]}

    def test_a_readme_repeating_the_card(self):
        readme = {"title": "Demucs", "tags": ["output:audio", "input:audio",
                                              "category:separation"]}
        self.assertEqual(readme_problems(readme, self.CARD), "")

    def test_tags_and_a_title_that_disagree(self):
        readme = {"title": "demucs", "tags": ["category:separation", "input:audio", "stems"]}
        problems = readme_problems(readme, self.CARD)
        self.assertIn("output:audio", problems)
        self.assertIn("stems", problems)
        self.assertIn("demucs", problems)

    def test_no_readme_is_nothing_to_compare(self):
        self.assertEqual(readme_problems(None, self.CARD), "")


class DeclaredFormatTest(unittest.TestCase):

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = str(Path(self.temp.name) / "speech.wav")
        soundfile.write(self.path, numpy.zeros((2400, 1)) + 0.1, 24000)

    def check(self, tags, inputs=None):
        controls = {"card": {"tags": tags},
                    "inputs": inputs or [{"label": "Text", "type": "text_box"}],
                    "outputs": [{"label": "Speech", "type": "audio_track"}]}
        VALIDATORS["declared_format"]({"Speech": self.path}, controls, {})

    def test_a_declaration_matching_the_audio(self):
        self.check(["sample-rate:24000", "channels:mono"])

    def test_a_missing_declaration(self):
        with self.assertRaises(AssertionError) as caught:
            self.check(["channels:mono"])
        self.assertIn("SampleRate(24000)", str(caught.exception))

    def test_a_wrong_declaration(self):
        with self.assertRaises(AssertionError):
            self.check(["sample-rate:44100", "channels:mono"])

    def test_a_model_given_audio_is_not_judged(self):
        with self.assertRaises(ValidatorNotApplicable):
            self.check([], inputs=[{"label": "Audio", "type": "audio_track"}])


if __name__ == "__main__":
    unittest.main()
