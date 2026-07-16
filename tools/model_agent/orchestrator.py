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
from typing import List, Optional

# ref detection ---------------------------------------------------------------

_GITHUB_RE = re.compile(r"github\.com[/:]+([A-Za-z0-9_.\-]+)/([A-Za-z0-9_.\-]+)")
_HF_SPACE_RE = re.compile(r"huggingface\.co/spaces/([A-Za-z0-9_.\-]+)/([A-Za-z0-9_.\-]+)")
_HF_MODEL_RE = re.compile(r"huggingface\.co/([A-Za-z0-9_.\-]+)/([A-Za-z0-9_.\-]+)")
_BARE_RE = re.compile(r"^([A-Za-z0-9_.\-]+)/([A-Za-z0-9_.\-]+)$")
# Any Space link, used to auto-discover a backend from a README / source blob.
_SPACE_LINK_RE = re.compile(
    r"huggingface\.co/spaces/([A-Za-z0-9_.\-]+)/([A-Za-z0-9_.\-]+)"
)

# What auto-execution the agent can perform end-to-end today. two-space still
# needs a separately hosted backend; remote/single/dual can be produced by the
# command pipeline below.
EXECUTABLE_MODES = frozenset({"remote", "single", "dual"})


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


# planning --------------------------------------------------------------------


@dataclass
class DeployPlan:
    """The chosen deployment strategy for a model reference."""

    mode: str  # remote | single | dual | two-space | remote-missing-backend
    backend_space: Optional[str]
    can_execute: bool
    rationale: List[str] = field(default_factory=list)
    blockers: List[str] = field(default_factory=list)
    guidance: List[str] = field(default_factory=list)

    def to_dict(self) -> dict:
        return {
            "mode": self.mode,
            "backend_space": self.backend_space,
            "can_execute": self.can_execute,
            "rationale": self.rationale,
            "blockers": self.blockers,
            "guidance": self.guidance,
        }


def decide_plan(
    *,
    kind: str,
    slug: str,
    backend_space: Optional[str] = None,
    decision_mode: Optional[str] = None,
    decision_blockers: Optional[List[str]] = None,
    prefer_existing_space: bool = True,
) -> DeployPlan:
    """Pick a deployment mode from the ref kind + (optional) classifier result.

    * A Space reference proxies to itself.
    * Otherwise, if a runnable backend Space was discovered (or supplied) and we
      prefer reuse, proxy to it -- the least-risk path (reuses tested code, and
      the frontend installs no conflicting deps).
    * Otherwise fall back to the deterministic classifier's mode.
    """

    blockers = list(decision_blockers or [])

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
        # Space was found -- we can't proxy to nothing.
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

    if mode == "dual":
        return DeployPlan(
            mode="dual",
            backend_space=None,
            can_execute=True,
            rationale=["dependency/version isolation needed (older Python / fragile native deps)."],
            blockers=blockers,
            guidance=[
                "This will generate a dual-interpreter Docker Space: pyHARP frontend "
                "+ isolated backend worker in one Space.",
            ],
        )

    # two-space
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

    steps.append(["generate-recipe", recipe_path, "--output", package_parent])
    deploy_step = ["deploy-space", package_dir, "--repo", target_repo]
    if plan.mode == "dual":
        deploy_step += ["--sdk", "docker"]
    steps.append(deploy_step)
    return steps
