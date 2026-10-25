"""
Checks for following the runtime stage of a space before it is validated.

These run locally with no network and no Hugging Face token:

    python -m unittest discover -s model_validation/tests -v
"""

from pathlib import Path
import sys
import time
from types import SimpleNamespace
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

import spaces


class WaitForRuntimeTest(unittest.TestCase):
    """Which stages a run waits out, and which it reports as they are."""

    def follow(self, *stages):
        """The stage a wait settles on, given the stages the space reports in turn."""
        reported = iter(stages)
        api = SimpleNamespace(get_space_runtime=lambda _: SimpleNamespace(stage=next(reported)))
        with mock.patch.object(spaces, "RUNTIME_POLL_INTERVAL", 0):
            return spaces.wait_for_runtime(api, "teamup-tech/a", time.time() + 5).stage

    def test_a_new_revision_starting_is_waited_out(self):
        # The old revision serves meanwhile, and stopping there failed the
        # space as "not running" the moment a run found it updating
        self.assertEqual(self.follow("RUNNING_APP_STARTING", "RUNNING"), "RUNNING")

    def test_a_build_is_waited_out(self):
        self.assertEqual(self.follow("BUILDING", "APP_STARTING", "RUNNING"), "RUNNING")

    def test_a_sleeping_space_is_reported_at_once(self):
        # A sleeping space only starts once something asks it to, so waiting
        # on it would spend the whole connect budget
        self.assertEqual(self.follow("SLEEPING", "RUNNING"), "SLEEPING")


class SpaceMetadataTest(unittest.TestCase):
    """One lookup serves both the wake and the README check."""

    def test_the_host_and_readme_come_from_one_request(self):
        calls = []

        def space_info(space_id):
            calls.append(space_id)
            return SimpleNamespace(host="https://a.hf.space",
                                   card_data=SimpleNamespace(tags=["input:audio"], title="A"))

        host, readme = spaces.space_metadata(SimpleNamespace(space_info=space_info), "teamup-tech/a")
        self.assertEqual(host, "https://a.hf.space")
        self.assertEqual(readme, {"tags": ["input:audio"], "title": "A"})
        self.assertEqual(calls, ["teamup-tech/a"])

    def test_a_failed_lookup_compares_nothing(self):
        def space_info(_):
            raise RuntimeError("429")

        self.assertEqual(spaces.space_metadata(SimpleNamespace(space_info=space_info), "x"),
                         (None, None))

    def test_a_wake_given_the_host_looks_it_up_no_further(self):
        api = SimpleNamespace(space_info=mock.Mock(side_effect=AssertionError("looked up")),
                              get_space_runtime=lambda _: SimpleNamespace(stage="RUNNING"))
        with mock.patch.object(spaces, "wake_space", return_value=(True, "")) as wake:
            stage, problem = spaces.start_space(api, "teamup-tech/a", time.time() + 60,
                                                host="https://a.hf.space")
        self.assertEqual((stage, problem), ("RUNNING", ""))
        wake.assert_called_once()
        self.assertEqual(wake.call_args.args[0], "https://a.hf.space")


if __name__ == "__main__":
    unittest.main()
