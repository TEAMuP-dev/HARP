"""
Shared utilities for HARP model validation: credentials, error rendering,
configuration, model discovery, and timeout handling.
"""

import dataclasses
import os
import sys
import threading
from pathlib import Path

import yaml


__all__ = [
    'get_token',
    'check_token',
    'scrub',
    'describe_exception',
    'run_with_timeout',
    'load_config',
    'check_config_keys',
    'check_model_names',
    'qualify',
    'qualify_keys',
    'Exclusions',
    'get_excluded',
    'discover_spaces'
]


# The settings each level of the configuration accepts, mirroring the
# reference in config.yml. Anything else is a mistake and is reported
# instead of being read by nothing at all.
CONFIG_KEYS = {"exclude", "include_extra", "common_test_cases",
               "synthesized_inputs", "overrides"}
MODEL_KEYS = {"connect_timeout", "process_timeout", "load_only",
              "skip_common_cases", "synthesized_inputs", "test_cases"}
CASE_KEYS = {"name", "process_timeout", "controls", "files",
             "synthesized_inputs", "expect", "validators"}


def qualify(model: str, owner: str) -> str:
    """
    Expand a bare model name using the owner of the tier being validated.

    Used for the names that can refer to either tier, which are `--exclude`
    and the config's `exclude`, `include_extra`, and `overrides` keys. Only
    one tier runs per invocation, so a bare name is unambiguous. It means
    the organization's space on a spaces run and the like-named example on
    an examples run. A name that already carries an owner is returned
    unchanged, pinning it to one tier. (`--spaces` names only spaces and
    `--local-examples` only examples, so neither takes the other's form.)

    Args:
        model (str): A space id, bare model name, or example key.
        owner (str): Owner to assume for bare names. This is the organization
            on a spaces run and "examples" on an examples run.

    Returns:
        model (str): The qualified name.
    """

    return model if "/" in model else f"{owner}/{model}"


def next_in_chain(exc: BaseException) -> BaseException | None:
    """
    The next exception to report from an exception's chain.

    An explicit `raise ... from <cause>` wins, and `raise ... from None`
    suppresses the implicit context, so a deliberately replaced error is not
    reported alongside the one it replaced.

    Args:
        exc (BaseException): The exception being described.

    Returns:
        cause (BaseException | None): The next exception, or None to stop.
    """

    if exc.__cause__ is not None:
        return exc.__cause__

    return None if exc.__suppress_context__ else exc.__context__


def describe_exception(exc: BaseException) -> str:
    """
    Render an exception with as much detail as it carries.

    Beyond the usual "Type: message", this surfaces a gradio AppError's
    `title` when the upstream app set an informative one, and follows the
    exception chain so the underlying cause is not lost. A Space launched
    with `show_error=False` deliberately returns only the exception class
    name, so for those there is nothing further to report.

    Args:
        exc (BaseException): The exception to describe.

    Returns:
        description (str): A single-line description of the exception.
    """

    parts = [f"{type(exc).__name__}: {exc}".strip()]

    # gradio's AppError carries a title alongside the message
    title = getattr(exc, "title", None)
    if title and title not in ("Error", str(exc)):
        parts.append(f"[{title}]")

    # Follow the chain so the root cause survives (bounded to stay one line)
    seen, cause = {id(exc)}, next_in_chain(exc)
    while cause is not None and id(cause) not in seen and len(parts) < 4:
        seen.add(id(cause))
        parts.append(f"caused by {type(cause).__name__}: {cause}".strip())
        cause = next_in_chain(cause)

    return " | ".join(parts)


def get_token(required: bool) -> str:
    """
    Read the Hugging Face access token from the HF_TOKEN environment variable.

    The token is intentionally never accepted as a command-line argument
    (argv is visible in process listings) and must never be committed.

    Args:
        required (bool): Exit with code 2 if the token is missing.

    Returns:
        token (str): The token, or an empty string when absent and optional.
    """

    token = os.environ.get("HF_TOKEN", "").strip()

    if required and not token:
        print("ERROR: HF_TOKEN environment variable is not set.", file=sys.stderr)
        print("Set it locally (export HF_TOKEN=...) or as a GitHub Actions secret.",
              file=sys.stderr)
        sys.exit(2)

    return token


def has_write_access(info: dict, org: str) -> bool | None:
    """
    Whether a token can write to an organization's spaces.

    Args:
        info (dict): The `whoami` payload describing the token.
        org (str): Organization the run will touch.

    Returns:
        allowed (bool | None): True or False when the token says plainly, and
            None when it does not. A fine-grained token lists its permissions
            per entity and the shapes vary, so one that says nothing about
            this organization is reported as unknown rather than guessed at.
    """

    access = (info.get("auth") or {}).get("accessToken") or {}
    role = access.get("role")

    if role in ("write", "admin"):
        return True
    if role == "read":
        return False

    # A fine-grained token lists permissions per entity, so the entry for this
    # organization is the one that decides. Its own account's entry says
    # nothing about the organization's spaces, unless the two are the same.
    for entry in (access.get("fineGrained") or {}).get("scoped") or []:
        if ((entry.get("entity") or {}).get("name")) == org:
            # Restarting a space is a repo write. Other write permissions a
            # token may carry, such as discussion.write, do not grant it
            return any(str(perm).startswith("repo.") and str(perm).endswith("write")
                       for perm in entry.get("permissions") or [])

    return None


def check_token(token: str, org: str, need_write: bool) -> bool:
    """
    Confirm the token works and covers what the run will ask of it.

    One `whoami` call answers both questions. A rejected token is fatal, since
    every request the run makes would fail the same way. Lacking write access
    is not fatal, since only the restart of a crashed space needs it, so that
    is reported as a warning naming the flag that turns restarts off.

    Args:
        token (str): The Hugging Face access token.
        org (str): Organization whose spaces the run will touch.
        need_write (bool): Whether the run may restart spaces.

    Returns:
        usable (bool): False when the token was rejected outright.
    """

    from huggingface_hub import HfApi

    try:
        info = HfApi().whoami(token=token)
    except Exception as exc:  # noqa: BLE001 - any failure means unusable
        # The head of the chain carries the reason. What follows it is the
        # HTTP status and request id, which say nothing a reader can act on
        reason = scrub(describe_exception(exc), token).split(" | ")[0]
        print(f"ERROR: HF_TOKEN was rejected. {' '.join(reason.split())}",
              file=sys.stderr)
        print("Check the token at https://huggingface.co/settings/tokens.",
              file=sys.stderr)
        return False

    role = ((info.get("auth") or {}).get("accessToken") or {}).get("role", "unknown")
    print(f"Token accepted for {info.get('name', 'unknown')} (role: {role})")

    if need_write and has_write_access(info, org) is False:
        print(f"WARNING: this token cannot restart spaces in {org}, so a crashed "
              f"or stopped space will be reported rather than recovered. Pass "
              f"--no-restart-failed to skip the attempt.", file=sys.stderr)

    return True


def scrub(text: str, token: str) -> str:
    """
    Remove the token from any string that might get printed or reported.

    Args:
        text (str): Text that may contain the token (e.g. an error message).
        token (str): The token to redact. An empty string disables scrubbing.

    Returns:
        text (str): The text with any token occurrence replaced.
    """

    return text.replace(token, "***HF_TOKEN***") if token else text


def run_with_timeout(fn, timeout: float, what: str):
    """
    Run fn() in a daemon thread, raising TimeoutError if it overruns.

    A daemon thread (not a ThreadPoolExecutor) is essential here: executor
    workers are non-daemon and are joined at interpreter shutdown, so a hung
    call leaked by a timeout would make the whole process hang after the
    final summary prints.

    Args:
        fn (callable): Zero-argument function to execute.
        timeout (float): Seconds to wait before giving up.
        what (str): Short description used in the timeout message.

    Returns:
        result: Whatever fn() returns.

    Raises:
        TimeoutError: If fn() does not finish within timeout seconds.
        Exception: Whatever fn() raised, re-raised in the calling thread.
    """

    outcome = {}

    def target():
        try:
            outcome["result"] = fn()
        except BaseException as exc:  # noqa: BLE001 - re-raised in caller
            outcome["error"] = exc

    thread = threading.Thread(target=target, daemon=True, name=f"timeout-{what}")
    thread.start()
    thread.join(timeout)

    if thread.is_alive():
        raise TimeoutError(f"{what} timed out after {int(timeout)}s")
    if "error" in outcome:
        raise outcome["error"]

    return outcome.get("result")


def load_config(path: Path) -> dict:
    """
    Load the validation configuration (see config.yml for the schema).

    Args:
        path (Path): Path to the YAML configuration file.

    Returns:
        config (dict): Parsed configuration, empty when the file is absent.
    """

    if not path.exists():
        return {}

    return yaml.safe_load(path.read_text()) or {}


def check_keys(mapping: dict | None, allowed: set, where: str) -> None:
    """
    Reject keys outside the set a level of the configuration accepts.

    Args:
        mapping (dict | None): The settings written at this level.
        allowed (set): The keys this level accepts.
        where (str): Where these settings were written, for reporting.

    Raises:
        ValueError: If any key is not one of the accepted settings.
    """

    unknown = sorted(set(mapping or {}) - allowed)

    if unknown:
        raise ValueError(f"unknown setting{'s' if len(unknown) > 1 else ''} "
                         f"{unknown} in {where}. Valid settings: "
                         f"{sorted(allowed)}")


def check_config_keys(config: dict) -> None:
    """
    Reject configuration keys that do not match actual settings.

    A misspelled key would otherwise be read by nothing. Writing `test_case`
    for `test_cases`, for instance, would leave the model running the synthesized
    default case while the file appears to say otherwise.

    Args:
        config (dict): Parsed configuration, with `overrides` already
            re-keyed by qualified name.

    Raises:
        ValueError: If any level of the configuration holds an unknown key.
    """

    check_keys(config, CONFIG_KEYS, "config.yml")

    for case in config.get("common_test_cases") or []:
        check_keys(case, CASE_KEYS,
                   f"common test case '{(case or {}).get('name', 'unnamed')}'")

    for model, entry in (config.get("overrides") or {}).items():
        check_keys(entry, MODEL_KEYS, f"'{model}'")
        for case in (entry or {}).get("test_cases") or []:
            check_keys(case, CASE_KEYS,
                       f"'{model}' test case "
                       f"'{(case or {}).get('name', 'unnamed')}'")


def check_model_names(config: dict, *cli_names) -> None:
    """
    Reject model names that were written as filesystem paths.

    A model is named `<name>` or `<owner>/<name>`, never by path. An example
    is identified by its directory name alone, so `examples/pitch_shifter`
    names that example wherever the directory happens to live. A path written
    in its place matches no model and would otherwise be ignored in silence,
    which for an `overrides` entry means every setting under it goes unused.

    Args:
        config (dict): Parsed configuration.
        *cli_names (list | None): Model names given on the command line.

    Raises:
        ValueError: If a name looks like a path rather than a model name.
    """

    named = (list(config.get("exclude") or [])
             + list(config.get("include_extra") or [])
             + list(config.get("overrides") or {}))

    for group in cli_names:
        named += list(group or [])

    bad = sorted({str(name) for name in named
                  if str(name).startswith("/") or str(name).count("/") > 1})

    if bad:
        subject = "model names" if len(bad) > 1 else "model name"
        verb = "look like paths" if len(bad) > 1 else "looks like a path"
        raise ValueError(
            f"{subject} {bad} {verb}. Name a model `<name>` or "
            f"`<owner>/<name>`, such as 'teamup-tech/pitch_shifter' or "
            f"'examples/pitch_shifter'. An example is named by its directory "
            f"alone, wherever it lives")


def qualify_keys(overrides: dict, owner: str) -> dict:
    """
    Re-key the config's `overrides` block by qualified model name.

    Args:
        overrides (dict): The config's `overrides` block.
        owner (str): Owner to assume for bare names (see qualify).

    Returns:
        overrides (dict): The block, re-keyed by qualified name.

    Raises:
        ValueError: If two keys resolve to the same model, which would
            otherwise silently discard one model's settings.
    """

    qualified = {}

    for model, entry in (overrides or {}).items():
        key = qualify(model, owner)
        if key in qualified:
            raise ValueError(f"config `overrides` names '{key}' twice (via "
                             f"'{model}'). Remove one of the entries")
        qualified[key] = entry

    return qualified


@dataclasses.dataclass(frozen=True)
class Exclusions:
    """
    The models to leave out, kept apart by which selections they apply to.

    A config `exclude` entry is a standing decision about a model, so it
    applies to whatever discovery turns up. Naming a model on the command line
    is a more specific instruction and overrides it, since asking for a model
    by name is unambiguous about wanting it. An `--exclude` on that same
    command line is equally specific, so it still applies.
    """

    config: frozenset
    cli: frozenset

    def excludes(self, model: str, named: bool = False) -> bool:
        """
        Whether a model is excluded, given how it came to be selected.

        Args:
            model (str): Qualified model name.
            named (bool): True when the model was named on the command line
                rather than found by discovery.

        Returns:
            excluded (bool): True when the model should be left out.
        """

        return model in self.cli or (not named and model in self.config)


def get_excluded(config: dict, cli_exclude: list | None, owner: str) -> Exclusions:
    """
    Collect the models excluded from validation, keeping the sources apart.

    Args:
        config (dict): Parsed configuration (its `exclude` list is used).
        cli_exclude (list | None): Models passed via --exclude, if any.
        owner (str): Owner to assume for bare names (see qualify).

    Returns:
        exclusions (Exclusions): Qualified names from each source.
    """

    return Exclusions(
        config=frozenset(qualify(model, owner)
                         for model in config.get("exclude", [])),
        cli=frozenset(qualify(model, owner) for model in (cli_exclude or [])))


def discover_spaces(api, org: str, config: dict, exclusions: Exclusions) -> list:
    """
    Enumerate the Hugging Face Spaces to validate.

    Args:
        api (HfApi): Authenticated Hugging Face API client.
        org (str): Organization whose spaces are discovered.
        config (dict): Parsed configuration (`include_extra` adds spaces
            outside the organization).
        exclusions (Exclusions): Models to leave out. These spaces are found
            rather than named, so every exclusion applies.

    Returns:
        space_ids (list): Sorted space ids to validate.
    """

    spaces = [s.id for s in api.list_spaces(author=org)]
    spaces += [qualify(s, org) for s in config.get("include_extra", [])
               if qualify(s, org) not in spaces]

    return sorted(s for s in spaces if not exclusions.excludes(s))
