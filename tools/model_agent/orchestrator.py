"""One-command deployment orchestration for the HARP model agent.

`recommend-mode`, `scaffold-remote-recipe`, `generate-recipe-from-llm`,
`generate-recipe`, and `deploy-space` are the building blocks; a new user should
not have to know which to run in which order. This module turns a single model
reference into a concrete **deployment plan** -- which mode to use and the exact
sub-commands to run -- so the CLI can print it (and optionally execute it).

Everything here is pure and deterministic (no network / LLM): the CLI does the
fetching/discovery and feeds the results in, which keeps the routing logic easy
to unit-test.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from typing import List, Mapping, Optional, Sequence, Tuple

from .classifier import DUAL_PYTHON_CEILING

# ref detection ---------------------------------------------------------------

_GITHUB_RE = re.compile(r"github\.com[/:]+([A-Za-z0-9_.\-]+)/([A-Za-z0-9_.\-]+)")
_HF_SPACE_RE = re.compile(r"huggingface\.co/spaces/([A-Za-z0-9_.\-]+)/([A-Za-z0-9_.\-]+)")
_HF_MODEL_RE = re.compile(r"huggingface\.co/([A-Za-z0-9_.\-]+)/([A-Za-z0-9_.\-]+)")
_BARE_RE = re.compile(r"^([A-Za-z0-9_.\-]+)/([A-Za-z0-9_.\-]+)$")
# Any Space link, used to auto-discover a backend from a README / source blob.
_SPACE_LINK_RE = re.compile(
    r"huggingface\.co/spaces/([A-Za-z0-9_.\-]+)/([A-Za-z0-9_.\-]+)"
)

# Modes the agent can execute end-to-end today. ``backend`` is Phase 1 of the
# two-Space workflow (plain Gradio Space that runs the model); the HARP frontend
# is a follow-up once that Space is Running (``deploy ... --space <backend>``).
EXECUTABLE_MODES = frozenset({"remote", "single", "dual", "backend"})

# Isolation approaches the user may choose when single/remote aren't viable.
ISOLATION_MODES = frozenset({"dual", "backend"})


def _strip_git(name: str) -> str:
    return name[:-4] if name.endswith(".git") else name


@dataclass
class RefTarget:
    """A parsed model reference."""

    kind: str  # "github" | "hf_space" | "hf_model"
    owner: str
    name: str

    @property
    def slug(self) -> str:
        return f"{self.owner}/{self.name}"


def detect_ref(ref: str, source: str = "auto") -> RefTarget:
    """Classify a model reference as a GitHub repo, an HF Space, or an HF model.

    ``source`` forces a kind for the ambiguous bare ``owner/name`` form (and to
    override URL sniffing): one of ``auto`` (default), ``github``, ``hf-space``,
    ``hf-model``.
    """

    ref = (ref or "").strip()
    if not ref:
        raise ValueError("empty model reference")

    if source == "github" or (source == "auto" and "github.com" in ref):
        match = _GITHUB_RE.search(ref)
        if match:
            return RefTarget("github", match.group(1), _strip_git(match.group(2)))

    if source == "hf-space" or (source == "auto" and "huggingface.co/spaces/" in ref):
        match = _HF_SPACE_RE.search(ref)
        if match:
            return RefTarget("hf_space", match.group(1), match.group(2))

    if source in ("hf-model", "auto") and "huggingface.co/" in ref:
        match = _HF_MODEL_RE.search(ref)
        if match:
            return RefTarget("hf_model", match.group(1), match.group(2))

    match = _BARE_RE.match(ref)
    if match:
        kind = {
            "github": "github",
            "hf-space": "hf_space",
            "hf-model": "hf_model",
            "auto": "hf_model",  # a bare owner/name defaults to an HF model repo
        }[source]
        name = _strip_git(match.group(2)) if kind == "github" else match.group(2)
        return RefTarget(kind, match.group(1), name)

    raise ValueError(f"could not parse model reference: {ref!r}")


def extract_space_links(text: str) -> List[str]:
    """All ``owner/space`` links found in a README/source blob, in order."""

    seen: List[str] = []
    for match in _SPACE_LINK_RE.finditer(text or ""):
        slug = f"{match.group(1)}/{match.group(2)}"
        if slug not in seen:
            seen.append(slug)
    return seen


def resolve_target_repo(repo: str, target: RefTarget) -> str:
    """Expand a target Space id, allowing an org-only shorthand.

    Lets a user pass just their org (e.g. ``teamup-tech``) and have the Space
    name derived from the model reference, so ``deploy .../pedalboard
    --repo teamup-tech`` targets ``teamup-tech/pedalboard``.

    * ``owner/name`` -> used verbatim.
    * ``owner`` (no slash) -> ``owner/<name-from-ref>``.
    * empty -> empty (the caller decides whether to prompt or error).

    A full ``https://huggingface.co/spaces/<owner>/<name>`` URL is also accepted
    and reduced to ``owner/name``.
    """

    repo = (repo or "").strip()
    if not repo:
        return ""
    match = _HF_SPACE_RE.search(repo)
    if match:
        return f"{match.group(1)}/{match.group(2)}"
    repo = repo.strip("/")
    if "/" in repo:
        return repo
    return f"{repo}/{target.name}"


# planning --------------------------------------------------------------------


@dataclass
class DeployPlan:
    """The chosen deployment strategy for a model reference."""

    mode: str  # remote | single | dual | backend | two-space | remote-missing-backend
    backend_space: Optional[str]
    can_execute: bool
    rationale: List[str] = field(default_factory=list)
    blockers: List[str] = field(default_factory=list)
    guidance: List[str] = field(default_factory=list)
    choices: List[str] = field(default_factory=list)

    def to_dict(self) -> dict:
        return {
            "mode": self.mode,
            "backend_space": self.backend_space,
            "can_execute": self.can_execute,
            "rationale": self.rationale,
            "blockers": self.blockers,
            "guidance": self.guidance,
            "choices": self.choices,
        }


def _python_floor_from_signals(signals: Optional[Mapping]) -> Optional[Tuple[int, int]]:
    if not isinstance(signals, Mapping):
        return None
    floor = signals.get("python_floor")
    if isinstance(floor, (list, tuple)) and len(floor) == 2:
        try:
            return (int(floor[0]), int(floor[1]))
        except (TypeError, ValueError):
            return None
    return None


def isolation_options(
    *,
    pip_installable: bool,
    python_floor: Optional[Tuple[int, int]] = None,
    allow_source_backend: bool = False,
) -> List[str]:
    """Approaches available when single/remote aren't viable.

    * ``dual`` -- one Docker Space (pyharp frontend + isolated backend). Needs a
      pip-installable repo and Python <= the dual base image ceiling.
    * ``backend`` -- Phase 1 of two-Space: plain Gradio Space that runs the model
      (HARP frontend later via ``--space``). Available when the repo is
      pip-installable **or** when ``allow_source_backend`` is set (GitHub script
      repos with no setup.py: the LLM inlines inference from fetched sources).
    """

    options: List[str] = []
    dual_ok = bool(pip_installable) and (
        python_floor is None or python_floor <= DUAL_PYTHON_CEILING
    )
    if dual_ok:
        options.append("dual")
    if pip_installable or allow_source_backend:
        options.append("backend")
    return options


def decide_plan(
    *,
    kind: str,
    slug: str,
    backend_space: Optional[str] = None,
    decision_mode: Optional[str] = None,
    decision_blockers: Optional[List[str]] = None,
    prefer_existing_space: bool = True,
    forced_mode: Optional[str] = None,
    pip_installable: bool = True,
    python_floor: Optional[Tuple[int, int]] = None,
    allow_source_backend: bool = False,
) -> DeployPlan:
    """Pick a deployment mode from the ref kind + (optional) classifier result.

    * A Space reference proxies to itself.
    * Otherwise, if a runnable backend Space was discovered (or supplied) and we
      prefer reuse, proxy to it -- the least-risk path (reuses tested code, and
      the frontend installs no conflicting deps).
    * Otherwise fall back to the deterministic classifier's mode.
    * For isolation cases (``dual`` / former ``two-space``), ``forced_mode`` may
      select ``dual`` or ``backend`` when both are available.
    """

    blockers = list(decision_blockers or [])
    choices = isolation_options(
        pip_installable=pip_installable,
        python_floor=python_floor,
        allow_source_backend=allow_source_backend,
    )

    if kind == "hf_space":
        return DeployPlan(
            mode="remote",
            backend_space=slug,
            can_execute=True,
            rationale=[f"{slug} is a running Space -> proxy to it (install no model deps)."],
        )

    if backend_space and prefer_existing_space:
        return DeployPlan(
            mode="remote",
            backend_space=backend_space,
            can_execute=True,
            rationale=[
                f"a runnable Space ({backend_space}) exists -> proxy to it "
                "(reuse tested code; frontend stays conflict-free)."
            ],
        )

    mode = decision_mode or "single"

    if mode == "single":
        return DeployPlan(
            mode="single",
            backend_space=None,
            can_execute=True,
            rationale=["no blockers -> one self-contained pyharp Space."],
        )

    if mode == "remote":
        # The classifier wants a proxy (deps conflict with pyharp) but no backend
        # Space was found -- we can't proxy to nothing. Offer isolation choices
        # when the repo can still be built as dual/backend.
        if choices:
            selected = _select_isolation_mode(
                forced_mode=forced_mode,
                default="dual" if "dual" in choices else "backend",
                choices=choices,
            )
            return _isolation_plan(selected, blockers=blockers, choices=choices)
        return DeployPlan(
            mode="remote-missing-backend",
            backend_space=None,
            can_execute=False,
            rationale=["dependencies conflict with pyharp, so the model needs a backend Space."],
            blockers=blockers,
            guidance=[
                "No runnable Space was found to proxy to. Duplicate/deploy a backend "
                "Space for this model, then re-run with --space <owner/space>.",
            ],
        )

    if mode in ("dual", "two-space") or forced_mode in ISOLATION_MODES:
        # Classifier recommended isolation (or the user forced dual/backend).
        if not choices:
            return DeployPlan(
                mode="two-space",
                backend_space=None,
                can_execute=False,
                rationale=[
                    "no single-Space path and the dual/backend paths can't build it "
                    "(not pip-installable and no existing Space)."
                ],
                blockers=blockers,
                guidance=[
                    "This needs a separately-hosted backend Space (the Woosh/Omnizart "
                    "pattern) plus a proxy frontend. Deploy the backend first, then "
                    "re-run with --space <owner/space> to build the frontend.",
                ],
            )
        default = "dual" if mode == "dual" and "dual" in choices else (
            "dual" if "dual" in choices else "backend"
        )
        # Prefer backend when the classifier said two-space (dual can't build).
        if mode == "two-space" and "backend" in choices:
            default = "backend"
        selected = _select_isolation_mode(
            forced_mode=forced_mode, default=default, choices=choices
        )
        return _isolation_plan(selected, blockers=blockers, choices=choices)

    # Unknown classifier mode -- treat as two-space guidance.
    return DeployPlan(
        mode="two-space",
        backend_space=None,
        can_execute=False,
        rationale=["no single-Space path and the dual base image can't build it."],
        blockers=blockers,
        guidance=[
            "This needs a separately-hosted backend Space (the Woosh/Omnizart pattern) "
            "plus a proxy frontend. Deploy the backend first, then re-run with "
            "--space <owner/space> to build the frontend.",
        ],
        choices=choices,
    )


def _select_isolation_mode(
    *,
    forced_mode: Optional[str],
    default: str,
    choices: Sequence[str],
) -> str:
    if forced_mode:
        if forced_mode not in ISOLATION_MODES:
            raise ValueError(
                f"forced_mode must be one of {sorted(ISOLATION_MODES)}, got {forced_mode!r}"
            )
        if forced_mode not in choices:
            raise ValueError(
                f"mode {forced_mode!r} is not available for this model "
                f"(available: {', '.join(choices) or 'none'})"
            )
        return forced_mode
    return default if default in choices else choices[0]


def _isolation_plan(
    mode: str, *, blockers: List[str], choices: List[str]
) -> DeployPlan:
    if mode == "dual":
        return DeployPlan(
            mode="dual",
            backend_space=None,
            can_execute=True,
            rationale=[
                "dependency/version isolation needed (older Python / fragile native deps)."
            ],
            blockers=blockers,
            guidance=[
                "This will generate a dual-interpreter Docker Space: pyHARP frontend "
                "+ isolated backend worker in one Space.",
            ],
            choices=list(choices),
        )
    # backend (Phase 1 of two-space)
    not_installable = any("not-pip-installable" in b for b in blockers)
    rationale = [
        "no single-Space path that coexists with pyharp -> deploy a plain Gradio "
        "backend Space that runs the model (Phase 1 of two-space)."
    ]
    if not_installable:
        rationale.append(
            "repo has no root packaging file: the backend will inline inference "
            "from the GitHub sources (no git+ install)."
        )
    return DeployPlan(
        mode="backend",
        backend_space=None,
        can_execute=True,
        rationale=rationale,
        blockers=blockers,
        guidance=[
            "This deploys the model-running backend only. When it shows Running, "
            "re-run deploy with --space <owner/backend-space> to build the pyharp "
            "frontend that proxies to it.",
        ],
        choices=list(choices),
    )


def build_steps(
    plan: DeployPlan,
    *,
    target: RefTarget,
    target_repo: str,
    recipe_path: str,
    package_parent: str,
    package_dir: str,
    user_token: bool = False,
    remote_api_name: str = "",
    ground_source: bool = True,
    inputs: str = "",
    outputs: str = "",
) -> List[List[str]]:
    """The concrete sub-command argv sequence that realizes an executable plan.

    ``package_dir`` is the predicted ``<package_parent>/<slug>`` folder that
    ``generate-recipe`` will create (the CLI recomputes it from the written
    recipe before the deploy step, so a wrong prediction here only affects the
    printed preview).
    """

    if plan.mode not in EXECUTABLE_MODES:
        return []

    source_flag = "--github" if target.kind == "github" else "--repo"

    steps: List[List[str]] = []
    if plan.mode == "remote":
        # Ground the proxy on the model's card/source when we have one (nicer UI,
        # LLM-picked endpoint); for a bare Space, the deterministic scaffold needs
        # no LLM key.
        if ground_source and target.kind in ("github", "hf_model"):
            recipe_step = [
                "generate-recipe-from-llm",
                "--remote-space", plan.backend_space or target.slug,
                "--remote-llm",
                source_flag, target.slug,
                "--output", recipe_path,
            ]
        else:
            recipe_step = [
                "scaffold-remote-recipe",
                plan.backend_space or target.slug,
                "--auto-endpoint",
                "--output", recipe_path,
            ]
        if remote_api_name:
            recipe_step += (
                ["--remote-api-name", remote_api_name]
                if "--remote-space" in recipe_step
                else ["--api-name", remote_api_name]
            )
        if user_token:
            recipe_step.append("--user-token")
        steps.append(recipe_step)
    elif plan.mode == "dual":
        recipe_step = [
            "generate-recipe-from-llm",
            source_flag, target.slug,
            "--dual",
            "--output", recipe_path,
        ]
        if inputs:
            recipe_step += ["--inputs", inputs]
        if outputs:
            recipe_step += ["--outputs", outputs]
        steps.append(recipe_step)
    elif plan.mode == "backend":
        recipe_step = [
            "generate-recipe-from-llm",
            source_flag, target.slug,
            "--backend",
            "--output", recipe_path,
        ]
        if inputs:
            recipe_step += ["--inputs", inputs]
        if outputs:
            recipe_step += ["--outputs", outputs]
        steps.append(recipe_step)
    else:  # single
        recipe_step = [
            "generate-recipe-from-llm",
            source_flag, target.slug,
            "--output", recipe_path,
        ]
        if inputs:
            recipe_step += ["--inputs", inputs]
        if outputs:
            recipe_step += ["--outputs", outputs]
        steps.append(recipe_step)

    gen_step = ["generate-recipe", recipe_path, "--output", package_parent]
    if plan.mode == "backend":
        gen_step.append("--backend")
    steps.append(gen_step)
    deploy_step = ["deploy-space", package_dir, "--repo", target_repo]
    if plan.mode == "dual":
        deploy_step += ["--sdk", "docker"]
    steps.append(deploy_step)
    return steps
