"""
Static checks of HARP Spaces against docs/DEPLOYMENT.md, the lint tier.

The spaces tier can only judge what a Space returns. A Space that loads its
model twice, reserves the GPU before checking its input, or installs a
requirement that will not resolve on its next rebuild returns the right
outputs today, so every one of those passes it. This tier reads each Space's
files from the Hub instead, without starting it, so it spends no ZeroGPU
allowance and wakes nothing.

Each finding names the rule it breaks (RULES in lint_base.py), and a rule is an
error or a warning. An error is a rule DEPLOYMENT.md states that the Space
breaks, and it fails the Space. A warning is a risk the guide leaves to
judgment, such as a requirement left unpinned, and is reported without failing
anything. A Space that breaks a rule for a reason records the exception in
config.yml under its `lint_exceptions`, with the reason, and an exception no
longer needed is reported in turn.

The rules on the app file live in lint_app.py and those on what the Space
installs in lint_requirements.py. This module reads a Space, runs both, and
applies its exceptions.
"""

import re
import time
from pathlib import Path

from lint_app import LocalModules, lint_app, lint_containers, lint_revisions
from lint_base import ERROR, RULES, Finding, SpaceFiles
from lint_requirements import (find_python_file, find_uv, read_dockerfile, read_requirements,
                               requirement_findings)
from results import ModelResult, PASS
from utils import describe_exception


__all__ = [
    'ERROR',
    'find_uv',
    'lint_files',
    'apply_exceptions',
    'check_lint_exceptions',
    'fetch_space',
    'lint_space'
]


def lint_files(space: SpaceFiles, uv: str | None = None) -> list:
    """
    Check one Space's files against docs/DEPLOYMENT.md.

    Args:
        space (SpaceFiles): The Space's metadata and file contents.
        uv (str | None): The uv executable, to resolve its requirements, or
            None to skip resolution.

    Returns:
        findings (list): Finding objects, errors first.
    """

    findings = []
    app = space.files.get(space.app_file)

    if app is None:
        findings.append(Finding("app-file", f"no app file ({space.app_file}) to check"))
        imports_spaces = False
    else:
        findings += lint_app(app, space.app_file, LocalModules(space.files), space.zerogpu)
        imports_spaces = bool(re.search(r"^\s*(import spaces|from spaces import)", app,
                                        re.MULTILINE))

    for name, text in space.files.items():
        if name.endswith(".py") and "/" not in name:
            findings += lint_revisions(text, name, space.listing)
            findings += lint_containers(text, name)

    specs = []
    if space.sdk == "docker":
        build = read_dockerfile(space.files.get("Dockerfile", ""))
        specs += build.specs
        for name in dict.fromkeys(build.requirement_files):
            specs += read_requirements(space.files.get(name, ""), name)
    else:
        for name in ("pre-requirements.txt", "requirements.txt"):
            specs += read_requirements(space.files.get(name, ""), name)

    findings += requirement_findings(space, specs, imports_spaces, uv)

    return sorted(findings, key=lambda f: (f.severity != ERROR, f.file, f.line or 0))


def apply_exceptions(findings: list, exceptions: dict | None) -> list:
    """
    Drop the findings a Space records an exception for, and report stale ones.

    Args:
        findings (list): The Space's findings.
        exceptions (dict | None): Its `lint_exceptions`, rule -> reason.

    Returns:
        findings (list): What remains, plus a stale-exception warning for each
            exception that matched nothing.
    """

    exceptions = exceptions or {}
    kept = [finding for finding in findings if finding.rule not in exceptions]
    fired = {finding.rule for finding in findings}

    for rule in sorted(set(exceptions) - fired):
        kept.append(Finding("stale-exception", f"lint_exceptions excuses '{rule}', which no "
                                               f"longer applies. Remove the exception"))

    return kept


def check_lint_exceptions(config: dict) -> None:
    """
    Reject a `lint_exceptions` entry that names no rule or gives no reason.

    Args:
        config (dict): Parsed configuration.

    Raises:
        ValueError: If an exception names an unknown rule or has no reason.
    """

    for model, entry in (config.get("overrides") or {}).items():
        exceptions = (entry or {}).get("lint_exceptions")
        if exceptions is None:
            continue
        if not isinstance(exceptions, dict):
            raise ValueError(f"lint_exceptions for '{model}' must map each rule to the "
                             f"reason it is excused")
        for rule, reason in exceptions.items():
            if rule not in RULES or rule == "stale-exception":
                raise ValueError(f"lint_exceptions for '{model}' names unknown rule "
                                 f"'{rule}'. Rules: {sorted(set(RULES) - {'stale-exception'})}")
            if not (isinstance(reason, str) and reason.strip()):
                raise ValueError(f"lint_exceptions for '{model}' excuses '{rule}' without "
                                 f"a reason. Say why the rule does not apply")


def lint_space(api, space_id: str, overrides: dict, uv: str | None):
    """
    Lint one Space end to end, as the lint tier does for each.

    Args:
        api (HfApi): Authenticated Hugging Face API client.
        space_id (str): The Space to lint.
        overrides (dict): Its entry in config.yml `overrides`.
        uv (str | None): The uv executable, or None to leave out resolution.

    Returns:
        result (ModelResult): The Space's findings, as `cases`, with FAIL
            when any is an error.
    """

    result = ModelResult(target=space_id, kind="lint")
    start = time.time()

    try:
        space = fetch_space(api, space_id)
        result.hardware = space.hardware
        findings = apply_exceptions(lint_files(space, uv), overrides.get("lint_exceptions"))
        result.cases = findings
        errors = [finding.describe() for finding in findings if finding.severity == ERROR]
        result.error = "; ".join(errors)
        if not errors:
            result.status = PASS
    except Exception as exc:  # noqa: BLE001 - reported as the Space's failure
        result.error = f"could not read the Space: {describe_exception(exc)}"
    finally:
        result.duration = round(time.time() - start, 1)

    return result


# The files the lint reads, beside the app and any top-level module
READ_FILES = ("requirements.txt", "pre-requirements.txt", "Dockerfile")


def fetch_space(api, space_id: str) -> SpaceFiles:
    """
    Read what the lint needs from a Space's repository, without starting it.

    Args:
        api (HfApi): Authenticated Hugging Face API client.
        space_id (str): The Space to read.

    Returns:
        space (SpaceFiles): Its metadata and the contents of the files read.
    """

    from huggingface_hub import hf_hub_download

    info = api.space_info(space_id)
    card = info.card_data
    get = (lambda key: card.get(key)) if card is not None else (lambda key: None)
    runtime = info.runtime

    space = SpaceFiles(
        space_id=space_id,
        sdk=str(get("sdk") or info.sdk or "gradio"),
        sdk_version=str(get("sdk_version") or ""),
        python_version=str(get("python_version") or ""),
        hardware=(getattr(runtime, "requested_hardware", None)
                  or getattr(runtime, "hardware", None) or "") if runtime else "",
        listing=[sibling.rfilename for sibling in info.siblings or []],
    )

    def read(name):
        path = hf_hub_download(space_id, name, repo_type="space", revision=info.sha,
                               token=api.token)
        return Path(path).read_text(encoding="utf-8", errors="replace")

    for name in READ_FILES:
        if name in space.listing:
            space.files[name] = read(name)

    if space.sdk == "docker":
        build = read_dockerfile(space.files.get("Dockerfile", ""))
        for name in build.requirement_files:
            if name in space.listing and name not in space.files:
                space.files[name] = read(name)
        # The CMD names the app, or a script that starts it
        app = find_python_file(build.command)
        script = next((word.strip('"[],') for word in build.command.split()
                       if word.strip('"[],').endswith(".sh")), "")
        if not app and Path(script).name in space.listing:
            app = find_python_file(read(Path(script).name))
        space.app_file = app if app in space.listing else "app.py"
    else:
        space.app_file = str(get("app_file") or "app.py")

    for name in space.listing:
        if (name.endswith(".py") and "/" not in name) or name == space.app_file:
            space.files[name] = read(name)

    return space
