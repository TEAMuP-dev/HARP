"""
Shared utilities for HARP model validation: credentials, error rendering,
configuration, model discovery, and timeout handling.
"""

import os
import sys
import threading
from pathlib import Path

import yaml


__all__ = [
    'get_token',
    'scrub',
    'describe_exception',
    'run_with_timeout',
    'load_config',
    'qualify',
    'qualify_keys',
    'get_excluded',
    'discover_spaces'
]


def qualify(model: str, owner: str) -> str:
    """
    Expand a bare model name using the owner of the tier being validated.

    Used for the names that can refer to either tier - `--exclude` and the
    config's `exclude`, `include_extra`, and `overrides` keys. Only one tier
    runs per invocation, so a bare name is unambiguous there: it means the
    organization's space on a spaces run and the like-named example on an
    examples run. A name that already carries an owner is returned unchanged,
    pinning it to one tier. (`--spaces` names only spaces and
    `--local-examples` only examples, so neither takes the other's form.)

    Args:
        model (str): A space id, bare model name, or example key.
        owner (str): Owner to assume for bare names - the organization on a
            spaces run, "examples" on an examples run.

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


def scrub(text: str, token: str) -> str:
    """
    Remove the token from any string that might get printed or reported.

    Args:
        text (str): Text that may contain the token (e.g. an error message).
        token (str): The token to redact; empty string disables scrubbing.

    Returns:
        text (str): The text with any token occurrence replaced.
    """

    return text.replace(token, "***HF_TOKEN***") if token else text


def run_with_timeout(fn, timeout: float, what: str):
    """
    Run fn() in a daemon thread; raise TimeoutError if it exceeds timeout.

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
        config (dict): Parsed configuration; empty when the file is absent.
    """

    if not path.exists():
        return {}

    return yaml.safe_load(path.read_text()) or {}


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
                             f"'{model}'); remove one of the entries")
        qualified[key] = entry

    return qualified


def get_excluded(config: dict, cli_exclude: list | None, owner: str) -> set:
    """
    Combine the models excluded from validation.

    Args:
        config (dict): Parsed configuration (its `exclude` list is used).
        cli_exclude (list | None): Models passed via --exclude, if any.
        owner (str): Owner to assume for bare names (see qualify).

    Returns:
        excluded (set): Qualified model names to leave out.
    """

    named = set(config.get("exclude", [])) | set(cli_exclude or [])

    return {qualify(model, owner) for model in named}


def discover_spaces(api, org: str, config: dict, excluded: set) -> list:
    """
    Enumerate the Hugging Face Spaces to validate.

    Args:
        api (HfApi): Authenticated Hugging Face API client.
        org (str): Organization whose spaces are discovered.
        config (dict): Parsed configuration (`include_extra` adds spaces
            outside the organization).
        excluded (set): Model keys to leave out.

    Returns:
        space_ids (list): Sorted space ids to validate.
    """

    spaces = [s.id for s in api.list_spaces(author=org)]
    spaces += [qualify(s, org) for s in config.get("include_extra", [])
               if qualify(s, org) not in spaces]

    return sorted(s for s in spaces if s not in excluded)
