"""Tests for deciding whether calling a space spends ZeroGPU allowance."""

import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from quota import spends_zerogpu


class SpendsZeroGPUTests(unittest.TestCase):
    def test_zerogpu_hardware_spends_allowance(self):
        self.assertTrue(spends_zerogpu("zero-a10g", []))

    def test_cpu_space_does_not(self):
        self.assertFalse(spends_zerogpu("cpu-basic", ["category:effects"]))

    def test_frontend_declaring_a_zerogpu_backend_spends_allowance(self):
        self.assertTrue(spends_zerogpu("cpu-basic", ["category:synthesis", "zerogpu"]))

    def test_unknown_hardware_and_tags_do_not(self):
        self.assertFalse(spends_zerogpu("", None))


if __name__ == "__main__":
    unittest.main()
