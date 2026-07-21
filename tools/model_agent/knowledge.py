"""Structured deployment knowledge base for the HARP model agent.

The agent gets better over time not by growing its prompt, but by *remembering*
what worked and what failed in a form it can query deterministically. This
module stores three kinds of knowledge on disk (under ``knowledge/``):

* **Deployments** (``registry.json``) -- one record per successful (or attempted)
  deployment: the repo, the chosen mode, the target Python, the dependency
  *fingerprint*, the detected blockers, and pointers to the working recipe and
  lockfile. These act as *priors*: before deploying a new repo, look up ones with
  the same dependency fingerprint to reuse the mode/pins that already worked.
* **Failure cases** (``failures.json``) -- ``{model, error_signature, diagnosis,
  fix}`` records. Retrieved by matching a new error against past ones.
* **Lockfiles** (``locks/<slug>.txt``) -- the exact, known-good resolved
  requirements for a deployment, so a re-deploy is reproducible and instant.

Retrieval is intentionally deterministic (exact repo, dependency fingerprint,
error signature) -- no embeddings needed for the common cases. The repair-rule
registry below is the *crystallized* form of repeated fixes: a fix applied twice
should live here as a rule, not as something the LLM has to recall.
"""

from __future__ import annotations

import hashlib
import json
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Mapping, Optional

from .recipe import _parse_install_line, collect_pip_requirements

JSON = Dict[str, Any]

_VCS_URL_PREFIXES = ("git+", "hg+", "svn+", "bzr+")


def _default_root() -> Path:
    return Path(__file__).resolve().parent / "knowledge"


def _slug(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "-", str(value).strip()).strip("-").lower()


def _canon_requirement(line: str) -> Optional[str]:
    """Canonicalize one pip line for fingerprinting, or None to drop it.

    ``name==1.2`` -> ``name==1.2``; ``Numpy < 2`` -> ``numpy<2``; a git/URL
    requirement -> its repo URL without a trailing ``@ref`` (so the same repo
    fingerprints identically regardless of the pinned commit); pip options such
    as ``--extra-index-url`` and comments are dropped.
    """

    text = str(line).strip()
    if not text or text.startswith("#") or text.startswith("-"):
        return None
    if text.startswith(_VCS_URL_PREFIXES) or "://" in text:
        # Normalize "git+https://host/owner/repo.git@ref" -> ".../repo"
        url = text.split()[0]
        url = url.rsplit("@", 1)[0] if "@" in url.split("://", 1)[-1] else url
        return url.removesuffix(".git").lower()
    parsed = _parse_install_line(text)
    if not parsed:
        return text.lower()
    name, spec, _exact = parsed
    return f"{name}{spec.replace(' ', '')}" if spec else name


def dependency_fingerprint(requirements: List[str]) -> str:
    """A stable 16-hex digest of a *set* of pip requirements.

    Order-independent and version-sensitive: two repos that install the same
    pinned dependency set share a fingerprint, so a new repo can be matched to a
    past deployment with the same stack. Pip options/comments are ignored.
    """

    canon = sorted({c for line in (requirements or []) if (c := _canon_requirement(line))})
    payload = "\n".join(canon)
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()[:16]


def fingerprint_for_recipe(recipe: Mapping[str, Any]) -> str:
    """Dependency fingerprint of the pins a recipe would actually install."""

    return dependency_fingerprint(collect_pip_requirements(recipe))


# --------------------------------------------------------------------------- #
# Repair rules: error signature -> deterministic fix suggestion.
#
# These are the "learned" fixes from past deployments, promoted from LLM memory
# to code. Each rule maps a regex over resolver/build/runtime error text to a
# structured suggestion the caller (or a human) can apply. Keeping them as data
# makes them testable and lets the failure store retrieve by the same signatures.
# --------------------------------------------------------------------------- #
@dataclass(frozen=True)
class RepairRule:
    name: str
    pattern: str  # regex, searched case-insensitively against the error text
    action: str  # machine-readable action id
    hint: str  # human-readable guidance

    def match(self, error_text: str) -> Optional[JSON]:
        m = re.search(self.pattern, error_text or "", re.IGNORECASE)
        if not m:
            return None
        result: JSON = {"rule": self.name, "action": self.action, "hint": self.hint}
        # Expose the most useful capture (e.g. the missing module name) if any.
        if m.groups():
            result["match"] = m.group(1)
        return result


REPAIR_RULES: List[RepairRule] = [
    RepairRule(
        name="missing-module",
        pattern=r"ModuleNotFoundError: No module named ['\"]([\w\.]+)['\"]",
        action="remove_unused_import_or_add_package",
        hint="First check whether the missing import came from copied source that "
        "the wrapper never uses; remove unused imports/modules before expanding "
        "the dependency graph. If the module is on the real inference path, add "
        "its distribution to the install set (map import names to PyPI names, "
        "e.g. cv2->opencv-python, sklearn->scikit-learn) and pin it alongside "
        "the sibling deps.",
    ),
    RepairRule(
        name="dependency-conflict",
        pattern=r"(?:ResolutionImpossible|conflicting dependencies|"
        r"has requirement .* but you'll have)",
        action="adjust_version_pins",
        hint="A pin violates a sibling package's declared constraint. Re-pin the "
        "offending package to the newest version that satisfies every declared "
        "bound (see the deterministic PyPI conflict check / find_dependency_conflicts).",
    ),
    RepairRule(
        name="pkg-resources-removed",
        pattern=r"No module named ['\"]pkg_resources['\"]",
        action="pin_setuptools_lt_81",
        hint="A legacy sdist imports pkg_resources at build time, removed in "
        "setuptools>=81. Pin 'setuptools<81' in the build environment "
        "(PIP_CONSTRAINT) -- the dual-interpreter Dockerfile already does this.",
    ),
    RepairRule(
        name="imp-removed",
        pattern=r"No module named ['\"]imp['\"]",
        action="use_older_python_or_dual",
        hint="The 'imp' module was removed in Python 3.12. Build this package "
        "under Python <=3.10 (dual-interpreter mode with an older backend_python; "
        "the dual base image cannot provision 3.11+).",
    ),
    RepairRule(
        name="setuptools-build-meta",
        pattern=r"Cannot import ['\"]setuptools\.build_meta['\"]",
        action="pin_setuptools_wheel",
        hint="The isolated build environment lacks a usable setuptools. Pin "
        "'setuptools'/'wheel' as build constraints, or add torchcodec/prebuilt "
        "wheels; unbounded numpy 2.x is a common trigger for source builds.",
    ),
    RepairRule(
        name="torch-no-distribution",
        pattern=r"No matching distribution found for torch==([\w\.\+]+)",
        action="use_cpu_wheel_or_remote",
        hint="The pinned torch build isn't on the default index. Use the CPU/CUDA "
        "wheel index (--extra-index-url https://download.pytorch.org/whl/cpu) or "
        "proxy to an existing backend Space (remote mode).",
    ),
    RepairRule(
        name="tensorflow-no-distribution",
        pattern=r"No matching distribution found for tensorflow(?:-cpu|-gpu)?==([\w\.\+]+)",
        action="pin_python_310_or_newer_tf",
        hint="That TensorFlow pin has no wheel for the Space's Python. For TF 1.x "
        "compat / tensorflow==2.15 research stacks, set framework.python_version "
        "to '3.10' (the agent writes python_version into the Space README). On "
        "Python 3.12+ only tensorflow>=2.16 (often 2.20+) is published -- bumping "
        "TF may break tf.compat.v1 code, so prefer pinning Python 3.10.",
    ),
    RepairRule(
        name="cuda-unavailable",
        pattern=r"(?:Torch not compiled with CUDA enabled|"
        r"CUDA (?:driver|error|unavailable)|no CUDA-capable device)",
        action="force_cpu_or_zerogpu",
        hint="Code assumes a GPU. Force CPU (map_location='cpu', device auto-"
        "detect) for a free Space, or target a ZeroGPU/GPU Space.",
    ),
    RepairRule(
        name="gradio-huggingface-hub-conflict",
        pattern=r"gradio .* and huggingface[_-]hub==?[\d\.]+ .* conflicting",
        action="switch_to_backend_frontend",
        hint="The model's deps can't co-exist with pyharp's gradio pin. Split into "
        "a backend Space (model deps) + a thin pyharp remote frontend, or use "
        "dual-interpreter mode.",
    ),
    RepairRule(
        name="gradio-pin-copied-into-pyharp-frontend",
        pattern=r"(?:gradio(?:\[[^\]]+\])?\s*={1,2}\s*(?!5\.28\.0)|"
        r"Cannot install .*gradio|ResolutionImpossible.*gradio)",
        action="remove_model_gradio_pin_from_frontend",
        hint="Do not copy a model Space's arbitrary gradio pin into a pyharp "
        "frontend. pyharp owns the frontend Gradio version (currently 5.28.0); "
        "keep model-specific Gradio requirements in a backend/remote Space, or "
        "use deploy-space --into-space so reconciliation aligns sdk_version and "
        "requirements safely.",
    ),
    RepairRule(
        name="not-pip-installable",
        pattern=r"does not appear to be a Python project",
        action="remote_or_backend",
        hint="The repo has no setup.py/pyproject.toml, so `pip install git+<repo>` "
        "fails. Deploy it as a plain-Gradio backend + remote frontend instead.",
    ),
    RepairRule(
        name="dual-backend-missing-output",
        pattern=r"(?:backend produced no ['\"]([^'\"]+)['\"] output|"
        r"missing output key\(s\):\s*([A-Za-z0-9_, \-]+))",
        action="fix_dual_worker_output_mapping",
        hint="The dual backend worker returned an outputs dict that does not match "
        "the recipe outputs. Ensure framework.dual.worker.body sets `outputs` with "
        "every recipe output name exactly, e.g. drums/bass/other/vocals for a "
        "4-stem wrapper. If the model only produces fewer stems, change the recipe "
        "outputs to match the model or explicitly derive/map the missing stems.",
    ),
    # -- ZeroGPU / `spaces` runtime pitfalls -------------------------------- #
    RepairRule(
        name="zerogpu-spaces-import-order",
        pattern=r"CUDA has been initialized before importing the spaces package",
        action="import_spaces_first",
        hint="Import `spaces` (inside its try/except) as the VERY FIRST import, "
        "before torch/gradio/diffusers or anything that touches CUDA.",
    ),
    RepairRule(
        name="zerogpu-cuda-outside-gpu-scope",
        pattern=r"(?:Low-level CUDA init|did not intercept a CUDA operation|"
        r"CUDA emulation mode)",
        action="keep_cuda_in_gpu_scope",
        hint="Something touched CUDA outside an @spaces.GPU-decorated call (often a "
        "background load thread). Keep loading CPU-only and move `.to(DEVICE)` "
        "into the GPU-decorated process_fn.",
    ),
    RepairRule(
        name="zerogpu-torch-version",
        pattern=r"torch version .* is not compatible with ZeroGPU|"
        r"CONFIG_ERROR:\s*torch",
        action="pin_zerogpu_supported_torch",
        hint="ZeroGPU supports only specific torch builds (e.g. 2.8.0/2.9.1/2.10.0/"
        "2.11.0). Pin torch/torchaudio to a supported one and leave other "
        "version-sensitive deps unpinned so pip resolves around it.",
    ),
    RepairRule(
        name="space-app-starting-timeout",
        pattern=r"APP_STARTING",
        action="background_load_or_redeploy",
        hint="For large models, synchronous download+init before launch() can exceed "
        "HF's startup timeout -- load weights in a background thread and flip a "
        "ready flag when done. For small models it's usually infra flakiness; "
        "push a trivial change to trigger a fresh deploy.",
    ),
    # -- Hugging Face push / storage / Space-card errors -------------------- #
    RepairRule(
        name="hf-auth-failed",
        pattern=r"[Aa]uthentication failed for .*huggingface\.co",
        action="use_hf_write_token",
        hint="Use a Hugging Face WRITE access token (not your account password) for "
        "git push and for HF_TOKEN.",
    ),
    RepairRule(
        name="hf-push-rejected",
        pattern=r"\[rejected\].*(?:fetch first|non-fast-forward)|"
        r"Updates were rejected because",
        action="reconcile_or_force_push_once",
        hint="The Space was created with an initial commit on the hub. Rebase "
        "(git pull --rebase) or, for a freshly generated Space, `git push --force` "
        "once.",
    ),
    RepairRule(
        name="hf-storage-limit",
        pattern=r"storage limit reached|Max:\s*1\s*GB|over the .*storage limit",
        action="host_checkpoint_separately",
        hint="The checkpoint exceeds the free-tier LFS quota. Host it in a separate "
        "HF model repo and hf_hub_download it at startup; if it was already "
        "committed, remove it from git history before re-pushing.",
    ),
    RepairRule(
        name="hf-hub-artifact-not-found-or-private",
        pattern=r"Repository Not Found for url: .*huggingface\.co/.*/resolve/.*|"
        r"Invalid username or password|private or gated repo",
        action="fix_checkpoint_source_or_token",
        hint="A runtime hf_hub_download checkpoint URL is missing, private, gated, "
        "or fetched with no readable token. Verify the repo id, filename, revision, "
        "and repo_type; if the artifact is private/gated, accept its terms and set "
        "HF_TOKEN with read access. If the upstream file moved or is unavailable, "
        "mirror the checkpoint into an accessible HF model repo and update the "
        "wrapper to download from that repo.",
    ),
    RepairRule(
        name="invalid-space-card-color",
        pattern=r'["\']?color(?:From|To)["\']?\s*must be one of',
        action="fix_space_card_color",
        hint="README.md colorFrom/colorTo must be one of: red, yellow, green, blue, "
        "indigo, purple, pink, gray.",
    ),
    # -- Checkpoint / torch.load pitfalls ----------------------------------- #
    RepairRule(
        name="torch-weights-only",
        pattern=r"(?:Weights only load failed|WeightsUnpickler|weights_only|"
        r"Unsupported global)",
        action="torch_load_weights_only_false",
        hint="PyTorch>=2.6 defaults torch.load(weights_only=True), which rejects "
        "non-tensor globals in older checkpoints. Pass weights_only=False for "
        "trusted checkpoints (the agent injects this shim into generated apps).",
    ),
    RepairRule(
        name="lightning-state-dict",
        pattern=r"(?:Missing key\(s\)|Unexpected key\(s\))? ?in ?loading ?state_dict|"
        r"Error\(s\) in loading state_dict|(?:Missing|Unexpected) key\(s\) in state_dict",
        action="use_lightning_state_dict_key",
        hint="PyTorch-Lightning checkpoints wrap weights under ['state_dict'] (often "
        "with a 'model.' prefix). Load ckpt['state_dict'] and strip the prefix "
        "rather than the raw checkpoint.",
    ),
    RepairRule(
        name="torchcodec-required",
        pattern=r"torchcodec",
        action="install_torchcodec_or_soundfile",
        hint="torchaudio>=2.8 offloads audio I/O to torchcodec. Install `torchcodec` "
        "(+ system ffmpeg) -- the agent adds these as companions -- or switch to "
        "soundfile (sf.read / sf.write).",
    ),
]


def match_repair_rules(error_text: str) -> List[JSON]:
    """Return every repair-rule suggestion whose signature matches ``error_text``."""

    out: List[JSON] = []
    for rule in REPAIR_RULES:
        hit = rule.match(error_text)
        if hit:
            out.append(hit)
    return out


# --------------------------------------------------------------------------- #
# The store.
# --------------------------------------------------------------------------- #
@dataclass
class KnowledgeBase:
    """File-backed store of deployments, failures, and lockfiles."""

    root: Path = field(default_factory=_default_root)

    @property
    def registry_path(self) -> Path:
        return self.root / "registry.json"

    @property
    def failures_path(self) -> Path:
        return self.root / "failures.json"

    @property
    def locks_dir(self) -> Path:
        return self.root / "locks"

    # ---- loading -------------------------------------------------------- #
    def _load(self, path: Path, key: str) -> List[JSON]:
        if not path.exists():
            return []
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (ValueError, OSError):
            return []
        if isinstance(data, dict):
            data = data.get(key, [])
        return [item for item in data if isinstance(item, dict)] if isinstance(data, list) else []

    def deployments(self) -> List[JSON]:
        return self._load(self.registry_path, "deployments")

    def failures(self) -> List[JSON]:
        return self._load(self.failures_path, "cases")

    def _save(self, path: Path, key: str, records: List[JSON]) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        payload = {"version": 1, key: records}
        path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    # ---- recording ------------------------------------------------------ #
    def record_deployment(self, record: Mapping[str, Any]) -> JSON:
        """Insert or update (keyed by repo+mode) a deployment record."""

        record = dict(record)
        repo = str(record.get("repo") or record.get("model_id") or "").strip()
        mode = str(record.get("mode") or "").strip()
        if not repo:
            raise ValueError("a deployment record needs a 'repo'")
        records = self.deployments()
        for index, existing in enumerate(records):
            if str(existing.get("repo")) == repo and str(existing.get("mode")) == mode:
                records[index] = {**existing, **record}
                self._save(self.registry_path, "deployments", records)
                return records[index]
        records.append(record)
        self._save(self.registry_path, "deployments", records)
        return record

    def record_failure(self, record: Mapping[str, Any]) -> JSON:
        """Append a failure/repair case (model, error_signature, diagnosis, fix)."""

        record = dict(record)
        if not str(record.get("error_signature") or "").strip():
            raise ValueError("a failure record needs an 'error_signature'")
        records = self.failures()
        records.append(record)
        self._save(self.failures_path, "cases", records)
        return record

    def write_lock(self, slug: str, requirements: List[str]) -> Path:
        self.locks_dir.mkdir(parents=True, exist_ok=True)
        path = self.locks_dir / f"{_slug(slug)}.txt"
        text = "\n".join(str(line) for line in requirements)
        path.write_text(text + ("\n" if text and not text.endswith("\n") else ""), encoding="utf-8")
        return path

    def lock_text(self, slug: str) -> Optional[str]:
        path = self.locks_dir / f"{_slug(slug)}.txt"
        return path.read_text(encoding="utf-8") if path.exists() else None

    # ---- retrieval ------------------------------------------------------ #
    def find_by_repo(self, repo: str) -> List[JSON]:
        needle = _slug(repo)
        return [
            record
            for record in self.deployments()
            if _slug(str(record.get("repo") or record.get("model_id") or "")) == needle
        ]

    def find_by_fingerprint(self, fingerprint: str) -> List[JSON]:
        return [
            record
            for record in self.deployments()
            if str(record.get("deps_fingerprint") or "") == fingerprint
        ]

    def find_similar(
        self, *, repo: str = "", requirements: Optional[List[str]] = None
    ) -> List[JSON]:
        """Retrieve prior deployments relevant to a new one.

        Exact-repo matches rank first, then dependency-fingerprint matches (same
        stack). De-duplicated, preserving that priority order.
        """

        seen: set = set()
        results: List[JSON] = []

        def _add(records: List[JSON]) -> None:
            for record in records:
                key = (str(record.get("repo")), str(record.get("mode")))
                if key not in seen:
                    seen.add(key)
                    results.append(record)

        if repo:
            _add(self.find_by_repo(repo))
        if requirements is not None:
            _add(self.find_by_fingerprint(dependency_fingerprint(requirements)))
        return results

    def find_failures_for_error(self, error_text: str) -> List[JSON]:
        """Past failure cases whose stored error_signature appears in ``error_text``."""

        low = (error_text or "").lower()
        return [
            case
            for case in self.failures()
            if str(case.get("error_signature") or "").lower() in low
        ]
