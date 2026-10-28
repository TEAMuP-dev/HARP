"""
The lint of what a Space installs: its requirement files, or for a Docker Space
whatever its Dockerfile installs, and whether they resolve the way its build
would install them. lint.py puts these findings beside those of lint_app.py.
"""

import dataclasses
import re
import shlex
import shutil
import subprocess
import tempfile
from pathlib import Path

from packaging.requirements import InvalidRequirement, Requirement
from packaging.utils import canonicalize_name

from lint_base import Finding, SpaceFiles


__all__ = [
    'Spec',
    'DockerBuild',
    'url_reference_pinned',
    'read_requirements',
    'read_dockerfile',
    'find_python_file',
    'find_uv',
    'matching_torchaudio',
    'requirement_findings'
]


# The releases ZeroGPU accepts for an exact torch pin, as docs/DEPLOYMENT.md
# lists them. Any other exact pin stops the Space before it starts, while a
# lower bound or no pin passes.
ZEROGPU_TORCH = {"2.8.0", "2.9.1", "2.10.0", "2.11.0", "2.12.1", "2.13.0"}


# torchaudio 2.11 is the final release, and the first to declare no torch
# requirement. It is built against the CUDA 13 runtime of torch 2.11 and later,
# so beside an earlier torch it fails to load ("libcudart.so.13"), while every
# earlier release requires the torch of its own version.
LAST_TORCHAUDIO = (2, 11)


# The Python a Gradio Space builds with when its README sets no
# python_version, as the tracebacks of such Spaces show: 3.10 on ZeroGPU and
# 3.13 everywhere else
DEFAULT_PYTHON_ZEROGPU = "3.10"


DEFAULT_PYTHON = "3.13"


# What the Gradio SDK image installs beside a Space's own requirements
GRADIO_SDK_PACKAGES = ("gradio[oauth,mcp]=={sdk_version}", "uvicorn>=0.14.0",
                       "websockets>=10.4", "spaces")


# The platform a Space builds on, for resolving its requirements
RESOLVE_PLATFORM = "x86_64-manylinux_2_28"


RESOLVE_TIMEOUT = 300


# A Git reference that names a moving branch rather than a commit or a tag
BRANCH_REFS = {"main", "master", "develop", "dev", "head", "trunk"}


# The pyharp repository, the only one a Space should install it from
PYHARP_REPOSITORY = "github.com/teamup-dev/pyharp"


@dataclasses.dataclass
class Spec:
    """One requirement, as a requirements file or a pip command names it."""

    text: str                        # the requirement, without comment or options
    line: int | None = None
    file: str = ""
    name: str = ""                   # normalized project name, when known
    url: str = ""                    # for a direct reference
    exact: str = ""                  # the version of an exact pin, or empty
    commented: bool = False          # a comment explains it, inline or above


def url_reference_pinned(url: str) -> bool:
    """
    Whether a direct reference names a commit or a release rather than a branch.

    Args:
        url (str): The URL, e.g. "git+https://github.com/o/r.git@v1.0".

    Returns:
        pinned (bool): True for a tag or commit, False for a branch or none.
    """

    lowered = url.lower()

    if lowered.startswith("git+") or ".git" in lowered.split("/")[-1]:
        ref = lowered.rsplit("@", 1)[1] if "@" in lowered.split("://", 1)[-1] else ""
        ref = ref.split("#")[0]
        return bool(ref) and ref not in BRANCH_REFS

    # An archive or file URL is pinned when its path names a commit or a version
    parts = [part for part in re.split(r"[/#?]", lowered) if part]
    if any(part in BRANCH_REFS or part.startswith(tuple(f"{b}." for b in BRANCH_REFS))
           for part in parts) or "refs/heads" in lowered:
        return False

    return any(re.fullmatch(r"[0-9a-f]{7,40}(\.(zip|tar\.gz))?", part) for part in parts) \
        or bool(re.search(r"\d+\.\d+", parts[-1] if parts else ""))


def parse_spec(text: str, line: int | None = None, file: str = "",
               commented: bool = False) -> Spec | None:
    """
    Parse one requirement into a Spec.

    Args:
        text (str): The requirement, comment and options already removed.
        line (int | None): Its line in the file, if it came from one.
        file (str): The file it came from, if any.
        commented (bool): Whether a comment explains it.

    Returns:
        spec (Spec | None): The parsed requirement, or None for text that is
            not one (a shell variable, say).
    """

    spec = Spec(text=text, line=line, file=file, commented=commented)

    # A bare URL names no project, so the name is taken from the repository
    if re.match(r"^(git\+)?[a-z]+://", text, re.IGNORECASE):
        spec.url = text
        stem = text.split("://", 1)[1].split("@")[0].rstrip("/").split("/")[-1]
        spec.name = canonicalize_name(stem.removesuffix(".git").split("#")[0]) \
            if stem and "." not in stem.removesuffix(".git") else ""
        if "egg=" in text:
            spec.name = canonicalize_name(text.split("egg=")[1].split("&")[0])
        return spec

    try:
        parsed = Requirement(text)
    except InvalidRequirement:
        return None

    spec.name = canonicalize_name(parsed.name)
    spec.url = parsed.url or ""
    specifiers = list(parsed.specifier)
    if len(specifiers) == 1 and specifiers[0].operator in ("==", "===") \
            and "*" not in specifiers[0].version:
        spec.exact = specifiers[0].version

    return spec


def read_requirements(text: str, file: str) -> list:
    """
    Read a requirements file into Specs.

    A requirement counts as explained when a comment sits at the end of its
    line or on the line directly above it, which is where docs/DEPLOYMENT.md
    asks for the reason a requirement is left unpinned.

    Args:
        text (str): The contents of the file.
        file (str): Its name, for reporting.

    Returns:
        specs (list): One Spec per requirement, in order.
    """

    specs, previous_comment = [], False

    for number, raw in enumerate(text.splitlines(), start=1):
        stripped = raw.strip()
        if not stripped:
            previous_comment = False
            continue
        if stripped.startswith("#"):
            previous_comment = True
            continue

        inline_comment = " #" in raw
        requirement = raw.split(" #")[0].strip()
        # pip ignores an option written on a requirement line, so it is
        # dropped the way pip would drop it
        if not requirement.startswith("-") and " --" in requirement:
            requirement = requirement.split(" --")[0].strip()

        if not requirement.startswith("-"):
            spec = parse_spec(requirement, number, file, previous_comment or inline_comment)
            if spec is not None:
                specs.append(spec)
        previous_comment = False

    return specs


# Options of `pip install` that take a value, so that the value is not read as
# a package
PIP_VALUE_OPTIONS = {"-r", "--requirement", "-c", "--constraint", "-i", "--index-url",
                     "--extra-index-url", "-f", "--find-links", "-t", "--target",
                     "--prefix", "--root", "--trusted-host", "--platform",
                     "--python-version", "--implementation", "--abi", "--cache-dir",
                     "--src", "--upgrade-strategy", "-C", "--config-settings",
                     "--global-option", "--no-binary", "--only-binary", "-e",
                     "--editable", "--progress-bar", "--log", "--report"}


@dataclasses.dataclass
class DockerBuild:
    """What a Dockerfile installs, and on which Python."""

    python: str = ""                 # the X.Y of a python:X.Y base image
    specs: list = dataclasses.field(default_factory=list)
                                     # Spec per package a `pip install` names
    requirement_files: list = dataclasses.field(default_factory=list)
                                     # files installed with `pip install -r`
    base_requirement_files: list = dataclasses.field(default_factory=list)
                                     # those installed by the pip of the base image
    command: str = ""                # what the CMD or ENTRYPOINT runs


def pip_arguments(tokens: list) -> tuple:
    """
    Recognize a `pip install` command, however the pip is reached.

    Args:
        tokens (list): The command, split as a shell would split it.

    Returns:
        args (list | None): What follows `install`, or None when the
            command is not a pip install.
        base (bool): Whether it is the pip of the base image itself, rather than
            one reached through a path or variable, such as that of a virtual
            environment.
    """

    # Leading variable assignments, e.g. PIP_CONSTRAINT="" pip install ...
    while tokens and re.match(r"^\w+=", tokens[0]):
        tokens = tokens[1:]

    if len(tokens) >= 2 and re.fullmatch(r"(.*/)?pip(\d(\.\d+)?)?", tokens[0]) \
            and tokens[1] == "install":
        return tokens[2:], "/" not in tokens[0] and "$" not in tokens[0]

    if len(tokens) >= 4 and re.fullmatch(r"(.*/)?python(\d(\.\d+)?)?", tokens[0]) \
            and tokens[1:4] == ["-m", "pip", "install"]:
        return tokens[4:], tokens[0] in ("python", "python3")

    return None, False


def read_dockerfile(text: str) -> DockerBuild:
    """
    Read what a Dockerfile installs and on which Python.

    Args:
        text (str): The contents of the Dockerfile.

    Returns:
        build (DockerBuild): The base Python, the packages and requirement
            files installed, and the command run.
    """

    joined = re.sub(r"\\\s*\n", " ", text)
    build = DockerBuild()

    for number, line in enumerate(joined.splitlines(), start=1):
        stripped = line.strip()
        keyword = stripped.split(" ", 1)[0].upper() if stripped else ""

        if keyword == "FROM" and not build.python:
            match = re.search(r"\bpython:(\d+\.\d+)", stripped)
            build.python = match.group(1) if match else ""

        if keyword in ("CMD", "ENTRYPOINT"):
            build.command = stripped.split(" ", 1)[1] if " " in stripped else ""

        if keyword != "RUN":
            continue

        for command in re.split(r"&&|\|\||;", stripped[len("RUN"):]):
            try:
                tokens = shlex.split(command)
            except ValueError:
                continue
            args, base = pip_arguments(tokens)
            if args is None:
                continue

            files, option = [], None
            for arg in args:
                if option is not None:
                    if option in ("-r", "--requirement"):
                        files.append(Path(arg).name)
                    option = None
                elif arg in PIP_VALUE_OPTIONS:
                    option = arg
                elif arg.startswith("--requirement="):
                    files.append(Path(arg.split("=", 1)[1]).name)
                elif arg.startswith("-r") and len(arg) > 2:
                    files.append(Path(arg[2:]).name)
                elif not arg.startswith("-"):
                    spec = parse_spec(arg, number, "Dockerfile")
                    if spec is not None:
                        build.specs.append(spec)
            build.requirement_files += files
            if base:
                build.base_requirement_files += files

    return build


def find_python_file(text: str) -> str:
    """
    The first Python file a command or script runs.

    Args:
        text (str): A CMD line or the contents of a shell script.

    Returns:
        name (str): e.g. "frontend_app.py", or empty when none is named.
    """

    match = re.search(r"([\w./-]+\.py)\b", text)

    return match.group(1).split("/")[-1] if match else ""


def find_uv() -> str | None:
    """
    Locate the uv executable, which resolves requirements the way a build would.

    Returns:
        path (str | None): The executable, or None when uv is not installed.
    """

    found = shutil.which("uv")
    if found:
        return found

    try:
        from uv import find_uv_bin
        return find_uv_bin()
    except Exception:  # noqa: BLE001 - not installed
        return None


def resolve(lines: list, python: str, uv: str) -> tuple:
    """
    Resolve requirement lines the way a Space's build would, without installing.

    Args:
        lines (list): Requirement and option lines, as a requirements file
            holds them.
        python (str): The Python version to resolve for, e.g. "3.10".
        uv (str): The uv executable.

    Returns:
        resolved (dict | None): Normalized project name -> version, or None
            when the requirements do not resolve.
        problem (str): Why they do not resolve, or empty.
    """

    with tempfile.TemporaryDirectory() as scratch:
        path = Path(scratch) / "requirements.txt"
        path.write_text("\n".join(lines) + "\n")
        try:
            completed = subprocess.run(
                [uv, "pip", "compile", str(path), "--python-version", python,
                 "--python-platform", RESOLVE_PLATFORM, "--no-header", "--no-annotate",
                 "--quiet", "--index-strategy", "unsafe-best-match"],
                capture_output=True, text=True, timeout=RESOLVE_TIMEOUT)
        except subprocess.TimeoutExpired:
            return None, f"resolution did not finish within {RESOLVE_TIMEOUT}s"

    if completed.returncode:
        # uv explains a failure over wrapped lines, then offers a hint, which
        # suggests flags rather than saying what is wrong
        explanation = " ".join(line.strip() for line in
                               completed.stderr.split("hint:")[0].splitlines())
        explanation = re.sub(r"\s+", " ", re.sub(r"\b(error|cause):\s*", "", explanation))
        return None, explanation.strip() or f"uv exited with {completed.returncode}"

    resolved = {}
    for line in completed.stdout.splitlines():
        match = re.match(r"^([A-Za-z0-9._-]+)(\[[^\]]*\])?==([^\s;]+)", line.strip())
        if match:
            resolved[canonicalize_name(match.group(1))] = match.group(3)

    return resolved, ""


def version_tuple(version: str) -> tuple:
    """
    The leading numeric parts of a version, e.g. (2, 13, 0) for "2.13.0+cpu".

    Args:
        version (str): A version string.

    Returns:
        parts (tuple): Its numeric release segments.
    """

    return tuple(int(part) for part in re.findall(r"\d+", version.split("+")[0])[:3])


def matching_torchaudio(torch: str, torchaudio: str) -> str:
    """
    The torchaudio to pin beside a torch release, when the given one cannot load.

    Args:
        torch (str): The torch version.
        torchaudio (str): The torchaudio version installed beside it.

    Returns:
        wanted (str): The torchaudio version to pin instead, or empty when
            the given one loads beside that torch.
    """

    torch_minor = version_tuple(torch)[:2]
    audio_minor = version_tuple(torchaudio)[:2]

    if audio_minor == torch_minor:
        return ""
    if audio_minor == LAST_TORCHAUDIO and torch_minor >= LAST_TORCHAUDIO:
        return ""

    return torch if torch_minor < LAST_TORCHAUDIO else \
        ".".join(str(part) for part in LAST_TORCHAUDIO) + ".0"


def requirement_findings(space: SpaceFiles, specs: list, imports_spaces: bool,
                         uv: str | None) -> list:
    """
    Check what a Space installs.

    Args:
        space (SpaceFiles): The Space.
        specs (list): Spec per requirement it installs, from every source.
        imports_spaces (bool): Whether its app imports spaces.
        uv (str | None): The uv executable, to resolve the requirements, or
            None to skip resolution.

    Returns:
        findings (list): Finding objects.
    """

    findings = []
    gradio_sdk = space.sdk == "gradio"
    by_name = {}
    for spec in specs:
        by_name.setdefault(spec.name, spec)

    # --- pyharp, at a release --------------------------------------------------
    pyharp = [spec for spec in specs
              if spec.name == "pyharp" or "/pyharp" in spec.url.lower()]
    if not pyharp:
        findings.append(Finding("pyharp-pin", "pyharp is not installed"))
    for spec in pyharp:
        if PYHARP_REPOSITORY not in spec.url.lower().replace("www.", ""):
            findings.append(Finding("pyharp-pin", f"pyharp comes from {spec.text} rather "
                                                  f"than the TEAMuP-dev repository",
                                    spec.line, spec.file))
        elif not url_reference_pinned(spec.url):
            findings.append(Finding("pyharp-pin", f"{spec.text} names a branch or no "
                                                  f"reference, so a rebuild can install a "
                                                  f"different pyharp", spec.line, spec.file))

    # --- Gradio from sdk_version, spaces beside it ----------------------------
    if gradio_sdk and "gradio" in by_name:
        spec = by_name["gradio"]
        findings.append(Finding("gradio-listed", "gradio is listed, though sdk_version in "
                                                 "the README is what pins it",
                                spec.line, spec.file))

    if imports_spaces and "spaces" not in by_name:
        findings.append(Finding("spaces-listed", "the app imports spaces, which nothing "
                                                 "installs"))

    if "spaces" in by_name and by_name["spaces"].exact:
        spec = by_name["spaces"]
        findings.append(Finding("spaces-exact", f"{spec.text} is an exact pin. Give spaces a "
                                                f"lower bound, since it talks to a ZeroGPU "
                                                f"service that changes with the platform",
                                spec.line, spec.file))

    # --- torch for ZeroGPU, and torchaudio beside it --------------------------
    torch = by_name.get("torch")
    if space.zerogpu and torch is not None and torch.exact \
            and torch.exact not in ZEROGPU_TORCH:
        accepted = ", ".join(sorted(ZEROGPU_TORCH, key=version_tuple))
        findings.append(Finding("zerogpu-torch", f"torch=={torch.exact} is not a release "
                                                 f"ZeroGPU accepts ({accepted})",
                                torch.line, torch.file))

    # --- direct references, at a commit or release ----------------------------
    for spec in specs:
        if spec.url and spec not in pyharp and not spec.commented \
                and not url_reference_pinned(spec.url):
            findings.append(Finding("url-unpinned", f"{spec.text} names a branch or no "
                                                    f"reference, and no comment says why",
                                    spec.line, spec.file))

    # spaces takes a lower bound, and the build tools a Dockerfile upgrades
    # before installing are not what the app runs on
    unpinned = [spec.text for spec in specs
                if not spec.exact and not spec.url and not spec.commented
                and spec.name not in ("spaces", "pyharp", "gradio", "pip", "setuptools",
                                      "wheel")]
    if unpinned:
        findings.append(Finding("unpinned", "not pinned to one version: " +
                                ", ".join(dict.fromkeys(unpinned))))

    # --- the requirements resolve ---------------------------------------------
    resolved = None
    lines = resolution_lines(space)
    if uv and lines is not None:
        python = resolution_python(space)
        if python:
            resolved, problem = resolve(lines, python, uv)
            if resolved is None:
                findings.append(Finding("resolves", f"the requirements do not resolve for "
                                                    f"Python {python}: {problem}"))

    # pyharp installs torchaudio through descript-audiotools, so every
    # environment with pyharp has one, and an unpinned one resolves to the
    # newest release. A Docker Space can keep its model in another
    # environment, without pyharp, so only a torch installed beside pyharp counts.
    pyharp_files = {spec.file for spec in pyharp}
    if resolved and "torch" in resolved and "torchaudio" in resolved:
        installed_torch, installed_audio = resolved["torch"], resolved["torchaudio"]
    elif resolved is None and torch is not None and torch.exact \
            and torch.file in pyharp_files:
        audio = by_name.get("torchaudio")
        installed_torch = torch.exact
        installed_audio = audio.exact if audio is not None and audio.exact else \
            ".".join(str(part) for part in LAST_TORCHAUDIO) + ".0"
    else:
        installed_torch = installed_audio = ""

    wanted = matching_torchaudio(installed_torch, installed_audio) if installed_torch else ""
    if wanted:
        findings.append(Finding("torchaudio-match",
                                f"torch {installed_torch} is installed beside torchaudio "
                                f"{installed_audio}, which does not load with it. Pin "
                                f"torchaudio=={wanted}, since pyharp installs it through "
                                f"descript-audiotools and an unpinned one resolves to the "
                                f"newest release", getattr(torch, "line", None),
                                getattr(torch, "file", "")))

    return findings


def resolution_python(space: SpaceFiles) -> str:
    """
    The Python into which the requirements of a Space are installed.

    Args:
        space (SpaceFiles): The Space.

    Returns:
        python (str): e.g. "3.10", or empty when it cannot be told.
    """

    if space.sdk == "docker":
        return read_dockerfile(space.files.get("Dockerfile", "")).python

    if space.python_version:
        return ".".join(str(space.python_version).split(".")[:2])

    return DEFAULT_PYTHON_ZEROGPU if space.zerogpu else DEFAULT_PYTHON


def resolution_lines(space: SpaceFiles) -> list | None:
    """
    The requirement lines a Space's build installs together, for resolving.

    A Gradio Space installs its requirements beside what the SDK image brings.
    A Docker Space is resolved only where the pip of the base image installs a
    requirements file. Its `pip install` lines run one after another, in an
    order a single resolution cannot reproduce, and a virtual environment it
    creates may run another Python than the base image.

    Args:
        space (SpaceFiles): The Space.

    Returns:
        lines (list | None): Requirement and option lines, or None when the
            build cannot be reproduced by one resolution.
    """

    def file_lines(name):
        lines = []
        for raw in space.files.get(name, "").splitlines():
            line = raw.split(" #")[0].strip()
            if not line or line.startswith("#"):
                continue
            if not line.startswith("-") and " --" in line:
                line = line.split(" --")[0].strip()
            # A nested -r points at a file the resolution does not have
            if line.startswith(("-r", "--requirement", "-c", "--constraint", "-e")):
                continue
            lines.append(line)
        return lines

    if space.sdk == "docker":
        build = read_dockerfile(space.files.get("Dockerfile", ""))
        installed = [name for name in build.base_requirement_files if name in space.files]
        if not installed or not build.python:
            return None
        return [line for name in installed for line in file_lines(name)]

    sdk = [package.format(sdk_version=space.sdk_version or "6.24.0")
           for package in GRADIO_SDK_PACKAGES]

    return file_lines("pre-requirements.txt") + file_lines("requirements.txt") + sdk
