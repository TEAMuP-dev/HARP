"""
Checks for when a run needs a token, and for what it says about the token it has.

These run locally with no network and no Hugging Face token:

    python -m unittest discover -s model_validation/tests -v
"""

import contextlib
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

import utils
import validate_models


def fine_grained(**permissions):
    """The `whoami` payload of a fine-grained token, given the permissions it holds per owner."""
    scoped = [
        {"entity": {"name": name}, "permissions": perms} for name, perms in permissions.items()
    ]

    return {
        "name": "someone",
        "auth": {"accessToken": {"role": "fineGrained", "fineGrained": {"scoped": scoped}}},
    }


class SpaceOwnersTest(unittest.TestCase):
    """Whose Spaces a run touches, which is whom a warning about the token names."""

    def test_named_spaces_give_their_owners(self):
        opts = SimpleNamespace(org="teamup-tech", spaces=["someone/model", "Demucs", "someone/b"])

        self.assertEqual(validate_models.space_owners(opts), ["someone", "teamup-tech"])

    def test_a_run_that_finds_its_spaces_touches_the_organization(self):
        opts = SimpleNamespace(org="teamup-tech", spaces=None)

        self.assertEqual(validate_models.space_owners(opts), ["teamup-tech"])


class CheckTokenTest(unittest.TestCase):
    """What a run reports about a token that cannot restart the Spaces it touches."""

    def warning(self, info, owners, need_write=True):
        """What checking a token with this `whoami` payload writes to the error stream."""
        api = SimpleNamespace(whoami=lambda token: info)
        errors = io.StringIO()

        with (
            mock.patch("huggingface_hub.HfApi", return_value=api),
            contextlib.redirect_stderr(errors),
            contextlib.redirect_stdout(io.StringIO()),
        ):
            self.assertTrue(utils.check_token("hf_x", owners, need_write))

        return errors.getvalue()

    def test_the_warning_names_the_owner_of_the_space(self):
        info = fine_grained(someone=["repo.content.read"])

        warning = self.warning(info, ["someone"])

        self.assertIn("cannot restart spaces in someone,", warning)
        self.assertNotIn("teamup-tech", warning)

    def test_only_the_owners_the_token_cannot_write_to_are_named(self):
        info = fine_grained(someone=["repo.write"], other=["repo.content.read"])

        self.assertIn("cannot restart spaces in other,", self.warning(info, ["other", "someone"]))

    def test_a_token_that_can_restart_draws_no_warning(self):
        self.assertEqual(self.warning(fine_grained(someone=["repo.write"]), ["someone"]), "")

    def test_an_owner_the_token_says_nothing_about_draws_no_warning(self):
        # Unknown is not reported as a refusal (see has_write_access)
        self.assertEqual(self.warning(fine_grained(someone=["repo.write"]), ["other"]), "")

    def test_a_run_that_restarts_nothing_draws_no_warning(self):
        info = fine_grained(someone=["repo.content.read"])

        self.assertEqual(self.warning(info, ["someone"], need_write=False), "")


class TokenRequirementTest(unittest.TestCase):
    """Which runs go ahead without a token."""

    def run_main(self, *arguments):
        """The exit code of a run without a token, with the tiers themselves replaced."""
        ran = mock.Mock(return_value=[])

        with (
            tempfile.TemporaryDirectory() as reports,
            mock.patch.object(
                sys, "argv", ["validate_models.py", "--output-dir", reports, *arguments]
            ),
            mock.patch.dict(os.environ, clear=False),
            mock.patch.object(validate_models, "validate_lint", ran),
            mock.patch.object(validate_models, "validate_spaces", ran),
            mock.patch.object(validate_models, "check_token") as check,
            mock.patch.object(validate_models, "write_reports", side_effect=SystemExit(0)),
            contextlib.redirect_stderr(io.StringIO()),
            contextlib.redirect_stdout(io.StringIO()),
            self.assertRaises(SystemExit) as stopped,
        ):
            os.environ.pop("HF_TOKEN", None)
            validate_models.main()

        return stopped.exception.code, ran, check

    def test_a_lint_of_named_spaces_needs_no_token(self):
        code, ran, check = self.run_main("--lint", "--spaces", "someone/model")

        self.assertEqual(code, 0)
        # The tier is handed an empty token, and no token is checked
        self.assertEqual(ran.call_args.args[-1], "")
        check.assert_not_called()

    def test_a_lint_that_finds_its_spaces_needs_a_token(self):
        code, ran, _ = self.run_main("--lint")

        self.assertEqual(code, 2)
        ran.assert_not_called()

    def test_running_a_named_space_needs_a_token(self):
        code, ran, _ = self.run_main("--spaces", "someone/model")

        self.assertEqual(code, 2)
        ran.assert_not_called()


if __name__ == "__main__":
    unittest.main()
