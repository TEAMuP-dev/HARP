"""Deterministic deployment-mode classifier for the HARP model agent.

The agent should pick a deployment architecture from *machine-checkable signals*,
not from an LLM's guess. This module extracts those signals from a repo's
dependency manifests / source and applies an ordered rule to recommend one of:

* ``single``    -- one self-contained pyharp Space (the default, preferred).
* ``remote``    -- a thin pyharp frontend proxying to an EXISTING backend Space
                   (still only one *new* Space).
* ``dual``      -- one dual-interpreter Docker Space (isolated backend venv).
                   Still ONE Space, so preferred over two-space.
* ``two-space`` -- our own backend Space + a pyharp remote frontend (last resort).

The ranking respects "avoid multiple Spaces": ``single`` and ``remote`` and
``dual`` each leave the user owning a single new Space; ``two-space`` is chosen
only when a genuine blocker rules the others out (notably: a required Python the
dual base image can't build, e.g. 3.12).
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import Any, Dict, List, Mapping, Optional, Tuple

from .recipe import _satisfies

# pyharp v0.3.0 hard-pins gradio==5.28.0. A model that needs a gradio which
# 5.28.0 does NOT satisfy cannot share a process with pyharp.
PYHARP_GRADIO_VERSION = "5.28.0"

# Beyond gradio, pyharp's stack constrains other SHARED packages that models
# frequently pin incompatibly -- the conflicts that repeatedly downgraded/broke
# real deployments (SoulX-Singer: NeMo needs protobuf~=5.29 but descript-audiotools
# -- a pyharp dep -- caps it <3.20; the gradio-5.28 era huggingface_hub is <1.0).
# We can't run a resolver here, so each pin is approximated by representative
# versions pyharp is known to accept: if a model's declared spec admits NONE of
# them, the two graphs can't be co-installed -> a single Space is blocked. This
# generalizes the gradio point-check without false-positiving on compatible ranges.
PYHARP_SHARED_PINS: Dict[str, Dict[str, Any]] = {
    "huggingface_hub": {
        "aliases": ("huggingface_hub", "huggingface-hub"),
        "pin": "<1.0 (gradio 5.28.0 era)",
        "ok_versions": ("0.28.1", "0.30.2", "0.34.0"),
    },
    "protobuf": {
        "aliases": ("protobuf",),
        "pin": "<3.20 (via descript-audiotools, a pyharp dep)",
        "ok_versions": ("3.19.6",),
    },
}

# The dual-interpreter Dockerfile builds its backend interpreter from Debian
# (python:3.10-slim-bullseye + apt pythonX.Y). Bullseye reliably provides only
# up to ~3.10, so treat >3.11 as out of reach for dual (this is why Woosh, which
# needs 3.12, becomes a two-space deploy).
DUAL_PYTHON_CEILING: Tuple[int, int] = (3, 11)
DUAL_PYTHON_FLOOR: Tuple[int, int] = (3, 7)
TARGET_PYTHON: Tuple[int, int] = (3, 10)

# Packages that (in our experience) build from source and routinely break a
# plain gradio-SDK Space: legacy sdists, Cython/native builds, or ones that need
# system libraries. Their presence argues for dual-interpreter isolation (or a
# prebuilt backend image) rather than a self-contained Space.
NATIVE_FRAGILE_PACKAGES = frozenset(
    {
        "crepe",
        "madmom",
        "aubio",
        "vamp",
        "vamphost",
        "pyfluidsynth",
        "fluidsynth",
        "soxbindings",
        "sox",
        "pyworld",
        "warp-rnnt",
        "ctcdecode",
        "fairseq",
        "pesq",
        "pystoi",
        "webrtcvad",
        "montreal-forced-aligner",
    }
)

_PY_FLOOR_RE = re.compile(r">=\s*3\.(\d+)")
_PY_EXACT_RE = re.compile(r"==\s*3\.(\d+)")
_CLASSIFIER_PY_RE = re.compile(r"Programming Language :: Python :: 3\.(\d+)")


def _canon(name: str) -> str:
    return name.strip().lower().replace("_", "-")


def _spec_for(blob: str, package: str) -> Optional[str]:
    """Version specifier a manifest declares for ``package``.

    Returns the specifier string (e.g. ``>=6.9.0``), ``""`` for a bare mention
    with no bound, or ``None`` if the package is not mentioned at all.
    """

    # Match "gradio", "gradio[oauth]", "gradio >= 6.9.0", 'gradio>=6.9.0', etc.
    pattern = re.compile(
        r"(?<![\w.\-])" + re.escape(package) + r"(?:\[[^\]]*\])?\s*([<>=!~][^,;'\")\]\s]*)?",
        re.IGNORECASE,
    )
    match = pattern.search(blob or "")
    if not match:
        return None
    return (match.group(1) or "").strip()


def _parse_requires_python(manifests: Mapping[str, str]) -> str:
    """Extract a ``requires-python`` specifier from the dependency manifests."""

    pyproject = manifests.get("pyproject.toml", "")
    setup_cfg = manifests.get("setup.cfg", "")
    setup_py = manifests.get("setup.py", "")

    for blob in (pyproject, setup_cfg):
        m = re.search(r"requires[-_]python\s*[:=]\s*['\"]([^'\"]+)['\"]", blob, re.IGNORECASE)
        if m:
            return m.group(1).strip()
    m = re.search(r"python_requires\s*=\s*['\"]([^'\"]+)['\"]", setup_py, re.IGNORECASE)
    if m:
        return m.group(1).strip()
    return ""


def _python_floor(requires_python: str, manifests: Mapping[str, str]) -> Optional[Tuple[int, int]]:
    """Best-effort minimum Python (3, minor) a repo supports."""

    minors: List[int] = []
    for m in _PY_FLOOR_RE.finditer(requires_python):
        minors.append(int(m.group(1)))
    for m in _PY_EXACT_RE.finditer(requires_python):
        minors.append(int(m.group(1)))
    if not minors:
        # Fall back to the highest "Python :: 3.x" trove classifier's LOWEST value
        # is a poor floor; instead take the minimum declared classifier version.
        blob = "\n".join(manifests.values())
        classifier_minors = [int(m.group(1)) for m in _CLASSIFIER_PY_RE.finditer(blob)]
        if classifier_minors:
            return (3, min(classifier_minors))
        return None
    return (3, min(minors))


def _python_ok_for_target(requires_python: str, target: Tuple[int, int]) -> bool:
    """Does ``requires_python`` admit the target interpreter? Unknown -> assume yes."""

    if not requires_python:
        return True
    version = f"{target[0]}.{target[1]}"
    try:
        return _satisfies(version, requires_python)
    except Exception:
        return True


_CUDA_HARDCODE_RE = re.compile(
    r"\.cuda\(\)|\.to\(\s*['\"]cuda|device\s*=\s*['\"]cuda|cuda:0|torch\.cuda\.set_device",
    re.IGNORECASE,
)
_CUDA_FALLBACK_RE = re.compile(
    r"is_available\(\)|map_location|device\s*=\s*['\"]cpu|CUDA_VISIBLE_DEVICES",
    re.IGNORECASE,
)


def _detect_cuda_hardcoded(sources: Mapping[str, str]) -> bool:
    """True if source hard-codes CUDA with no visible CPU fallback."""

    blob = "\n".join(sources.values())
    if not _CUDA_HARDCODE_RE.search(blob):
        return False
    return not _CUDA_FALLBACK_RE.search(blob)


def _shared_dependency_conflicts(blob: str) -> List[Dict[str, str]]:
    """Model requirements that conflict with a pyharp-shared pin (besides gradio).

    For each shared package, take the model's declared specifier and check whether
    ANY version pyharp is known to accept satisfies it. If none do, the model's
    requirement and pyharp's constraint are disjoint -> they can't co-install.
    A bare mention (no version bound) or an unparseable spec is treated as
    compatible (no conflict).
    """

    conflicts: List[Dict[str, str]] = []
    for pkg, info in PYHARP_SHARED_PINS.items():
        spec: Optional[str] = None
        for alias in info["aliases"]:
            found = _spec_for(blob, alias)
            if found:  # first alias with an actual version bound wins
                spec = found
                break
        if not spec:
            continue
        try:
            compatible = any(_satisfies(v, spec) for v in info["ok_versions"])
        except Exception:
            compatible = True  # can't parse -> don't block on it
        if not compatible:
            conflicts.append(
                {"package": pkg, "requirement": spec, "pyharp_pin": str(info["pin"])}
            )
    return conflicts


@dataclass
class DeploymentSignals:
    """Machine-checkable facts that drive the mode decision."""

    pip_installable: bool = True
    requires_python: str = ""
    python_floor: Optional[Tuple[int, int]] = None
    python_ok: bool = True
    gradio_requirement: Optional[str] = None
    gradio_conflict: bool = False
    dependency_conflicts: List[Dict[str, str]] = field(default_factory=list)
    native_fragile: List[str] = field(default_factory=list)
    cuda_hardcoded: bool = False
    has_existing_space: bool = False
    weights_noncommercial: bool = False

    def to_dict(self) -> Dict[str, Any]:
        return {
            "pip_installable": self.pip_installable,
            "requires_python": self.requires_python,
            "python_floor": (
                f"{self.python_floor[0]}.{self.python_floor[1]}" if self.python_floor else None
            ),
            "python_ok": self.python_ok,
            "gradio_requirement": self.gradio_requirement,
            "gradio_conflict": self.gradio_conflict,
            "dependency_conflicts": [dict(c) for c in self.dependency_conflicts],
            "native_fragile": list(self.native_fragile),
            "cuda_hardcoded": self.cuda_hardcoded,
            "has_existing_space": self.has_existing_space,
            "weights_noncommercial": self.weights_noncommercial,
        }


_PACKAGING_MARKERS = ("setup.py", "pyproject.toml", "setup.cfg")


def analyze_signals(
    *,
    manifests: Optional[Mapping[str, str]] = None,
    sources: Optional[Mapping[str, str]] = None,
    card: Optional[Mapping[str, Any]] = None,
    has_existing_space: bool = False,
    target_python: Tuple[int, int] = TARGET_PYTHON,
) -> DeploymentSignals:
    """Compute deployment signals from a repo's manifests / source / card."""

    manifests = dict(manifests or {})
    sources = dict(sources or {})
    blob = "\n".join(manifests.values())

    # pip-installable: a ROOT packaging file is required for `pip install git+repo`.
    pip_installable = bool(manifests) or _card_is_pip_installable(card)
    if manifests:
        pip_installable = any(name in _PACKAGING_MARKERS for name in manifests)
        if not pip_installable and "requirements.txt" in manifests and len(manifests) == 1:
            # Only a requirements.txt was found (no setup.py/pyproject): fall back
            # to the file-list heuristic rather than declaring it uninstallable.
            pip_installable = _card_is_pip_installable(card)

    requires_python = _parse_requires_python(manifests)
    floor = _python_floor(requires_python, manifests)
    python_ok = _python_ok_for_target(requires_python, target_python)

    gradio_spec = _spec_for(blob, "gradio")
    gradio_conflict = False
    if gradio_spec:  # a bounded gradio requirement
        try:
            gradio_conflict = not _satisfies(PYHARP_GRADIO_VERSION, gradio_spec)
        except Exception:
            gradio_conflict = False

    dependency_conflicts = _shared_dependency_conflicts(blob)

    native = sorted(
        pkg for pkg in NATIVE_FRAGILE_PACKAGES if _spec_for(blob, pkg) is not None
    )

    cuda_hardcoded = _detect_cuda_hardcoded(sources)

    noncommercial = _detect_noncommercial(card)

    return DeploymentSignals(
        pip_installable=pip_installable,
        requires_python=requires_python,
        python_floor=floor,
        python_ok=python_ok,
        gradio_requirement=gradio_spec,
        gradio_conflict=gradio_conflict,
        dependency_conflicts=dependency_conflicts,
        native_fragile=native,
        cuda_hardcoded=cuda_hardcoded,
        has_existing_space=has_existing_space,
        weights_noncommercial=noncommercial,
    )


def _card_is_pip_installable(card: Optional[Mapping[str, Any]]) -> bool:
    """Mirror cli._repo_is_pip_installable but tolerant of a missing card."""

    if not isinstance(card, Mapping):
        return True
    files = card.get("files")
    if not isinstance(files, list) or not files:
        return True
    for entry in files:
        path = str(entry).strip().lstrip("./")
        if "/" in path:
            continue
        if path.lower() in _PACKAGING_MARKERS:
            return True
    return False


_NONCOMMERCIAL_RE = re.compile(r"cc-by-nc|non-?commercial|research[ -]only", re.IGNORECASE)


def _detect_noncommercial(card: Optional[Mapping[str, Any]]) -> bool:
    if not isinstance(card, Mapping):
        return False
    meta = card.get("meta") if isinstance(card.get("meta"), Mapping) else card
    license_name = str((meta or {}).get("license") or "")
    return bool(_NONCOMMERCIAL_RE.search(license_name))


# A single artifact this large won't fit a free/ZeroGPU Space comfortably (build
# storage + ephemeral runtime), so it's worth a heads-up before you deploy.
LARGE_WEIGHTS_GB = 5.0

_SIZE_RE = re.compile(r"(?P<num>\d+(?:\.\d+)?)\s*(?P<unit>TB|GiB|GB|MiB|MB)\b", re.IGNORECASE)
# Strong GPU-requirement cues: accelerator model names, VRAM, explicit "needs a GPU".
_GPU_RE = re.compile(
    r"\b(A100|H100|H200|V100|A6000|A40|A10G?|L4|L40S?|T4|RTX\s?\d{3,4}|VRAM|"
    r"requires?\s+(?:a\s+)?gpu|gpu\s+(?:is\s+)?required|needs?\s+(?:a\s+)?gpu)\b",
    re.IGNORECASE,
)
# A size followed by one of these is a memory spec, not a weight/download size.
_MEMORY_TRAILER_RE = re.compile(r"^\s*(?:of\s+)?(?:v?ram|memory)\b", re.IGNORECASE)


def _size_to_gb(num: float, unit: str) -> float:
    unit = unit.lower()
    if unit == "tb":
        return num * 1024.0
    if unit in ("gb", "gib"):
        return num
    return num / 1024.0  # mb / mib


def detect_resource_warnings(text: str) -> Dict[str, Any]:
    """Scan README/manifest/source text for size + GPU-requirement cues.

    Pure and deterministic. Returns the largest advertised weight/download size
    (in GB, memory specs like "24 GB VRAM" excluded) plus raw size/GPU evidence.
    """

    text = text or ""
    largest_gb: Optional[float] = None
    size_evidence: List[str] = []
    for match in _SIZE_RE.finditer(text):
        trailing = text[match.end() : match.end() + 12]
        if _MEMORY_TRAILER_RE.match(trailing):
            continue  # "24 GB VRAM" / "16 GB RAM" -> a memory spec, not an artifact
        gb = _size_to_gb(float(match.group("num")), match.group("unit"))
        if gb < LARGE_WEIGHTS_GB:
            continue
        size_evidence.append(match.group(0).strip())
        if largest_gb is None or gb > largest_gb:
            largest_gb = gb

    gpu_evidence: List[str] = []
    seen = set()
    for match in _GPU_RE.finditer(text):
        token = re.sub(r"\s+", " ", match.group(0).strip())
        key = token.lower()
        if key not in seen:
            seen.add(key)
            gpu_evidence.append(token)

    return {
        "largest_size_gb": round(largest_gb, 1) if largest_gb is not None else None,
        "size_evidence": size_evidence[:5],
        "gpu_evidence": gpu_evidence[:5],
    }


def resource_headsup(warnings: Mapping[str, Any]) -> Optional[str]:
    """Compose a one-line, non-blocking heads-up from ``detect_resource_warnings``."""

    parts: List[str] = []
    size_gb = warnings.get("largest_size_gb")
    if isinstance(size_gb, (int, float)) and size_gb >= LARGE_WEIGHTS_GB:
        parts.append(f"~{size_gb:g} GB of weights/assets advertised")
    gpu = list(warnings.get("gpu_evidence") or [])
    if gpu:
        parts.append("GPU cues (" + ", ".join(gpu[:3]) + ")")
    if not parts:
        return None
    return (
        "this model looks heavy: "
        + "; ".join(parts)
        + ". A free/ZeroGPU Space likely can't host it -- target a paid GPU Space, "
        "or prefer a remote proxy to an existing GPU Space. (Not a blocker.)"
    )


@dataclass
class ModeDecision:
    mode: str
    rationale: List[str] = field(default_factory=list)
    blockers: List[str] = field(default_factory=list)
    recommendations: List[str] = field(default_factory=list)
    signals: Dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "mode": self.mode,
            "rationale": self.rationale,
            "blockers": self.blockers,
            "recommendations": self.recommendations,
            "signals": self.signals,
        }


def _dual_can_build(signals: DeploymentSignals) -> bool:
    """Can the dual-interpreter Docker Space realistically build this backend?"""

    if not signals.pip_installable:
        # dual installs the model via a git+ line in backend_pip, which also needs
        # a root packaging file; without one, dual can't install it either.
        return False
    floor = signals.python_floor
    if floor and floor > DUAL_PYTHON_CEILING:
        return False
    return True


def classify(signals: DeploymentSignals) -> ModeDecision:
    """Recommend a deployment mode from computed signals.

    Ordered so that a single Space wins whenever nothing blocks it, then the
    lowest-ownership fallback that a genuine blocker permits.
    """

    blockers: List[str] = []
    if not signals.pip_installable:
        blockers.append("not-pip-installable (no root setup.py/pyproject.toml)")
    if not signals.python_ok:
        floor = signals.python_floor
        detail = f" (requires {signals.requires_python or (floor and f'>=3.{floor[1]}') or '?'})"
        blockers.append(f"python-incompatible with {TARGET_PYTHON[0]}.{TARGET_PYTHON[1]}{detail}")
    if signals.gradio_conflict:
        blockers.append(
            f"gradio-conflict (model needs gradio {signals.gradio_requirement}; "
            f"pyharp pins {PYHARP_GRADIO_VERSION})"
        )
    for conflict in signals.dependency_conflicts:
        blockers.append(
            f"{conflict['package']}-conflict (model needs {conflict['package']} "
            f"{conflict['requirement']}; pyharp needs {conflict['pyharp_pin']})"
        )
    if signals.native_fragile:
        blockers.append("native-build-fragile: " + ", ".join(signals.native_fragile))

    rationale: List[str] = []
    recommendations: List[str] = []

    if signals.cuda_hardcoded:
        recommendations.append(
            "source hard-codes CUDA: force CPU (map_location='cpu' / device auto-detect) "
            "for a free Space, or target a ZeroGPU/GPU Space."
        )
    if signals.weights_noncommercial:
        recommendations.append(
            "weights look non-commercial (CC-BY-NC / research-only): mark the Space "
            "license accordingly and keep it non-commercial."
        )

    if not blockers:
        rationale.append("no blockers: a single self-contained pyharp Space should work.")
        mode = "single"
    elif signals.has_existing_space:
        rationale.append(
            "blockers exist but an existing backend Space is available -> proxy to it "
            "(one new Space, no backend to maintain)."
        )
        mode = "remote"
    elif _dual_can_build(signals):
        rationale.append(
            "blockers are dependency/version isolation issues the dual-interpreter "
            "Docker Space resolves -> still one Space."
        )
        mode = "dual"
        if signals.native_fragile:
            recommendations.append(
                "if the maintainers publish a working Docker image, a two-space Docker "
                "backend built FROM it can be less work than rebuilding the native "
                "stack in dual (see the omnizart example)."
            )
    else:
        reason = []
        if not signals.pip_installable:
            reason.append("not pip-installable and no existing Space")
        floor = signals.python_floor
        if floor and floor > DUAL_PYTHON_CEILING:
            reason.append(
                f"requires Python >=3.{floor[1]}, above the dual base image's ceiling "
                f"3.{DUAL_PYTHON_CEILING[1]}"
            )
        rationale.append(
            "no single-Space path and dual can't build it ("
            + "; ".join(reason or ["unresolvable blockers"])
            + ") -> deploy our own backend Space + a pyharp remote frontend."
        )
        mode = "two-space"

    return ModeDecision(
        mode=mode,
        rationale=rationale,
        blockers=blockers,
        recommendations=recommendations,
        signals=signals.to_dict(),
    )


def recommend_mode(
    *,
    manifests: Optional[Mapping[str, str]] = None,
    sources: Optional[Mapping[str, str]] = None,
    card: Optional[Mapping[str, Any]] = None,
    has_existing_space: bool = False,
    target_python: Tuple[int, int] = TARGET_PYTHON,
) -> ModeDecision:
    """Convenience: analyze signals then classify in one call."""

    signals = analyze_signals(
        manifests=manifests,
        sources=sources,
        card=card,
        has_existing_space=has_existing_space,
        target_python=target_python,
    )
    return classify(signals)
