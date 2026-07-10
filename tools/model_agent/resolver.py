"""Dependency-resolution pre-check for the HARP model agent.

Before committing to a 10-minute Docker build, try to *resolve* the install set
and surface conflicts up front. Two layers, cheapest first:

1. The deterministic PyPI metadata check (``recipe.find_dependency_conflicts``,
   wired in the CLI) catches "pin vs a sibling's declared bound" conflicts with
   no install at all.
2. This module runs ``pip install --dry-run`` to let pip's real resolver find
   conflicts the metadata check can't (deep transitive graphs). It can target the
   deploy environment (Linux / Python 3.10, wheels-only) instead of the host, so
   the check reflects what the Space will actually see.

Everything is best-effort: if pip can't run or the network is unreachable the
result is marked ``skipped`` rather than failing the pipeline. The resolver-log
parser is pure (and unit-tested) so conflict extraction works even offline.
"""

from __future__ import annotations

import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Sequence

# Requirements pip cannot resolve for a *foreign* platform (wheels-only): source
# builds and VCS/URL installs. We report them as "unchecked" in target mode.
_UNCHECKABLE_PREFIXES = ("git+", "hg+", "svn+", "bzr+", "-")
DEFAULT_TARGET_PYTHON = "3.10"
DEFAULT_TARGET_PLATFORM = "manylinux2014_x86_64"

Runner = Callable[[Sequence[str]], "subprocess.CompletedProcess"]


def _default_runner(cmd: Sequence[str]) -> "subprocess.CompletedProcess":
    return subprocess.run(list(cmd), capture_output=True, text=True, timeout=300)


@dataclass
class ResolutionResult:
    ok: bool
    skipped: bool = False
    return_code: Optional[int] = None
    conflicts: List[Dict[str, Any]] = field(default_factory=list)
    unchecked: List[str] = field(default_factory=list)
    log: str = ""
    command: List[str] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "ok": self.ok,
            "skipped": self.skipped,
            "return_code": self.return_code,
            "conflicts": self.conflicts,
            "unchecked": self.unchecked,
            "command": self.command,
            "log_tail": self.log[-2000:] if self.log else "",
        }


# --------------------------------------------------------------------------- #
# Pure parsing of pip's resolver output.
# --------------------------------------------------------------------------- #
_CANNOT_INSTALL_RE = re.compile(
    r"Cannot install (?P<what>.+?) because these package versions have conflicting dependencies",
    re.IGNORECASE,
)
_RESOLUTION_IMPOSSIBLE_RE = re.compile(r"ResolutionImpossible", re.IGNORECASE)
_NO_MATCHING_RE = re.compile(
    r"No matching distribution found for (?P<req>[^\s]+)", re.IGNORECASE
)
_CAUSE_HEADER_RE = re.compile(r"The conflict is caused by:", re.IGNORECASE)


def parse_resolution_conflicts(text: str) -> List[Dict[str, Any]]:
    """Extract structured conflict records from a pip resolver log.

    Handles the three common shapes: ``ResolutionImpossible`` with a "The
    conflict is caused by:" cause list, "Cannot install X and Y because ...
    conflicting dependencies", and "No matching distribution found for ...".
    """

    text = text or ""
    conflicts: List[Dict[str, Any]] = []

    lines = text.splitlines()
    for index, line in enumerate(lines):
        if _CAUSE_HEADER_RE.search(line):
            causes: List[str] = []
            for follow in lines[index + 1 :]:
                stripped = follow.strip()
                if not stripped:
                    break
                # Cause lines are indented; a de-indented "To fix this..." ends it.
                if not (follow.startswith((" ", "\t"))):
                    break
                if stripped.lower().startswith(("to fix", "1.", "2.")):
                    break
                causes.append(stripped)
            if causes:
                conflicts.append({"kind": "resolution-impossible", "causes": causes})

    for match in _CANNOT_INSTALL_RE.finditer(text):
        what = match.group("what").strip()
        # "-r requirements.txt (line 1) and librosa==0.10.1" -> the concrete pins
        packages = [
            token
            for token in re.split(r"\s+and\s+", what)
            if not token.lower().startswith("-r ")
        ]
        conflicts.append({"kind": "cannot-install", "summary": what, "packages": packages})

    for match in _NO_MATCHING_RE.finditer(text):
        conflicts.append({"kind": "no-distribution", "requirement": match.group("req")})

    return conflicts


def has_conflict_signal(text: str) -> bool:
    """True if the log shows any resolver conflict (vs. a network/other error)."""

    return bool(
        _RESOLUTION_IMPOSSIBLE_RE.search(text or "")
        or _CANNOT_INSTALL_RE.search(text or "")
        or _NO_MATCHING_RE.search(text or "")
    )


# --------------------------------------------------------------------------- #
# Running pip --dry-run.
# --------------------------------------------------------------------------- #
def _split_checkable(requirements: Sequence[str], *, wheels_only: bool) -> "tuple[List[str], List[str]]":
    """Partition requirements into (checkable, unchecked-for-this-mode)."""

    if not wheels_only:
        return list(requirements), []
    checkable: List[str] = []
    unchecked: List[str] = []
    for line in requirements:
        text = str(line).strip()
        if not text or text.startswith("#"):
            continue
        if text.startswith(_UNCHECKABLE_PREFIXES) or "://" in text:
            unchecked.append(text)
        else:
            checkable.append(text)
    return checkable, unchecked


def build_dry_run_command(
    requirements_file: str,
    *,
    python_executable: Optional[str] = None,
    target_python: Optional[str] = None,
    target_platform: Optional[str] = None,
) -> List[str]:
    """Build a ``pip install --dry-run`` command.

    When ``target_python``/``target_platform`` are set, pip resolves for that
    foreign environment, which forces ``--only-binary=:all:`` (pip cannot run a
    foreign platform's source builds).
    """

    python_executable = python_executable or sys.executable
    cmd = [
        python_executable,
        "-m",
        "pip",
        "install",
        "--dry-run",
        "--ignore-installed",
        "--disable-pip-version-check",
        "-r",
        requirements_file,
    ]
    if target_python:
        cmd += ["--python-version", target_python]
    if target_platform:
        cmd += ["--platform", target_platform]
    if target_python or target_platform:
        cmd += ["--only-binary=:all:"]
    return cmd


def resolve_requirements(
    requirements: Sequence[str],
    *,
    python_executable: Optional[str] = None,
    target_python: Optional[str] = None,
    target_platform: Optional[str] = None,
    runner: Optional[Runner] = None,
) -> ResolutionResult:
    """Attempt to resolve ``requirements`` with ``pip install --dry-run``.

    Target mode (``target_python``/``target_platform`` set) resolves wheels-only
    for the deploy environment and reports git/URL/sdist lines as ``unchecked``.
    Host mode (neither set) resolves on the current interpreter and can follow
    source/VCS builds. Best-effort: pip/network failures return ``skipped=True``.
    """

    requirements = [str(line) for line in requirements if str(line).strip()]
    if not requirements:
        return ResolutionResult(ok=True, log="(no requirements to resolve)")

    wheels_only = bool(target_python or target_platform)
    checkable, unchecked = _split_checkable(requirements, wheels_only=wheels_only)
    if not checkable:
        return ResolutionResult(
            ok=True, unchecked=unchecked, log="(nothing checkable in target mode)"
        )

    runner = runner or _default_runner
    tmp = Path(tempfile.mkdtemp(prefix="harp_resolve_")) / "requirements.txt"
    tmp.write_text("\n".join(checkable) + "\n", encoding="utf-8")

    cmd = build_dry_run_command(
        str(tmp),
        python_executable=python_executable,
        target_python=target_python,
        target_platform=target_platform,
    )

    try:
        proc = runner(cmd)
    except (OSError, subprocess.SubprocessError) as exc:
        return ResolutionResult(
            ok=True, skipped=True, unchecked=unchecked, command=cmd,
            log=f"(pip dry-run could not run: {exc})",
        )

    log = (getattr(proc, "stdout", "") or "") + (getattr(proc, "stderr", "") or "")
    return_code = getattr(proc, "returncode", None)

    if return_code == 0:
        return ResolutionResult(
            ok=True, return_code=0, unchecked=unchecked, command=cmd, log=log
        )

    if has_conflict_signal(log):
        return ResolutionResult(
            ok=False,
            return_code=return_code,
            conflicts=parse_resolution_conflicts(log),
            unchecked=unchecked,
            command=cmd,
            log=log,
        )

    # Nonzero but no resolver-conflict signature: usually network/index/proxy or
    # a wheels-only miss for a package with no wheel on the target. Don't fail the
    # pipeline on an inconclusive run.
    return ResolutionResult(
        ok=True, skipped=True, return_code=return_code, unchecked=unchecked, command=cmd, log=log
    )
