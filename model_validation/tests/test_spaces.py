"""
Checks for following a space's runtime stage before it is validated.

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


if __name__ == "__main__":
    unittest.main()
