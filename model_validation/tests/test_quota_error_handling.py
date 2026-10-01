"""Tests for deciding whether a quota error stops a validation run."""

import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from utils import check_config_keys, quota_error_exhausts_validation_token


class QuotaErrorHandlingTests(unittest.TestCase):
    def test_quota_error_defaults_to_validation_token(self):
        self.assertTrue(quota_error_exhausts_validation_token({}))

    def test_downstream_quota_error_does_not_stop_other_spaces(self):
        overrides = {"quota_error_exhausts_validation_token": False}
        self.assertFalse(quota_error_exhausts_validation_token(overrides))

    def test_override_is_a_valid_model_setting(self):
        check_config_keys({
            "overrides": {
                "teamup-tech/proxy": {
                    "quota_error_exhausts_validation_token": False,
                },
            },
        })


if __name__ == "__main__":
    unittest.main()
