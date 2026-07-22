from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Iterable, List, Mapping, Optional
from urllib.error import HTTPError, URLError

from .agent import (
    HARP_GRADIO_VERSION,
    DeploySpaceError,
    EndpointProbeError,
    lint_generated_app,
    HarpModelAgent,
    SmokeTestResult,
    VenvSetupError,
    write_json,
)
from .classifier import detect_resource_warnings, recommend_mode, resource_headsup
from .knowledge import (
    KnowledgeBase,
    fingerprint_for_recipe,
    match_repair_rules,
)
from .orchestrator import (
    ISOLATION_MODES,
    RefTarget,
    build_steps,
    decide_plan,
    detect_ref,
    extract_space_links,
    isolation_options,
    resolve_target_repo,
)
from .llm import (
    LLMError,
    RecipeDraft,
    RecipeGenerationContext,
    complete_recipe,
    default_examples,
    generate_recipe,
    pick_remote_endpoint,
    provider_from_env,
    refine_remote_recipe,
)
from .recipe import (
    RecipeError,
    _slug,
    apply_dependency_fixes,
    attach_vendor_files,
    build_package_from_recipe,
    collect_pip_requirements,
    ensure_backend_runtime_defaults,
    find_dependency_conflicts,
    guess_primary_endpoint,
    lint_recipe_requirements,
    remote_recipe_from_api_info,
    render_app_from_recipe,
)


def _add_venv_flag(parser: argparse.ArgumentParser) -> None:
    parser.add_argument(
        "--venv",
        action="store_true",
        help="Run the smoke-test in an isolated, cached venv built from the package's "
        "requirements.txt (never installs into your active environment).",
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="model-agent",
        description="Discover, probe, and package HARP-compatible open-source models.",
    )
    parser.add_argument("--timeout", type=float, default=120.0, help="Network timeout in seconds.")

    subparsers = parser.add_subparsers(dest="command", required=True)

    discover = subparsers.add_parser("discover", help="Search Hugging Face Spaces for candidates.")
    discover.add_argument("--query", default="", help="Search query.")
    discover.add_argument("--author", default="", help="Restrict results to a Hugging Face author/org.")
    discover.add_argument("--tag", action="append", default=[], help="Require a Hugging Face tag/filter.")
    discover.add_argument("--limit", type=int, default=25, help="Maximum candidates to request.")
    discover.add_argument("--output", type=Path, help="Optional JSON output path.")
    discover.add_argument(
        "--all",
        action="store_true",
        help="Keep candidates that are not obviously open-source Gradio Spaces.",
    )

    scaffold_remote = subparsers.add_parser(
        "scaffold-remote-recipe",
        help="Probe a backend Space's Gradio API and scaffold a remote-backend "
        "(proxy) recipe: a thin pyharp frontend that calls the Space via "
        "gradio_client. Best for models whose deps conflict with pyharp.",
    )
    scaffold_remote.add_argument(
        "space",
        help="Backend Gradio Space id or URL (e.g. owner/space) to proxy to.",
    )
    scaffold_remote.add_argument(
        "--api-name",
        default="",
        help="Backend named endpoint to call (e.g. /predict). Required only if the "
        "Space exposes more than one; otherwise the sole endpoint is used.",
    )
    scaffold_remote.add_argument(
        "--auto-endpoint",
        action="store_true",
        help="When the Space exposes several endpoints and --api-name is omitted, "
        "auto-pick the primary inference endpoint with a deterministic heuristic "
        "(prefers /predict-like names that return media; ignores UI controls like "
        "/interrupt, /toggle_*) instead of erroring. For a smarter choice on "
        "ambiguous Spaces, use generate-recipe-from-llm --remote-space --remote-llm.",
    )
    scaffold_remote.add_argument(
        "--user-token",
        action="store_true",
        help="Add an optional masked 'Hugging Face token' control to the frontend UI. "
        "When a user supplies a token, the backend call is made as that user, so "
        "ZeroGPU quota is charged to their account (with fallback to this Space's "
        "HF_TOKEN secret). Use for public/multi-user frontends of a ZeroGPU backend.",
    )
    scaffold_remote.add_argument("--output", type=Path, help="Optional recipe JSON output path.")

    render_recipe = subparsers.add_parser(
        "render-recipe",
        help="Render a pyharp app.py from a wrapper recipe JSON file.",
    )
    render_recipe.add_argument("recipe", type=Path, help="Recipe JSON file.")
    render_recipe.add_argument("--output", type=Path, help="Optional app.py output path.")

    generate_recipe = subparsers.add_parser(
        "generate-recipe",
        help="Write app.py, requirements, and manifest from a wrapper recipe JSON file.",
    )
    generate_recipe.add_argument("recipe", type=Path, help="Recipe JSON file.")
    generate_recipe.add_argument(
        "--output",
        type=Path,
        default=Path("artifacts/model_agent/generated"),
        help="Generated package output directory.",
    )
    generate_recipe.add_argument(
        "--smoke-test",
        action="store_true",
        help="After writing, launch app.py and verify HARP controls (runs downloaded code).",
    )
    generate_recipe.add_argument(
        "--backend",
        action="store_true",
        help="Render a plain-Gradio BACKEND Space (no pyharp) exposing /predict, for the "
        "two-Space workflow. Forces framework.backend on the recipe before rendering.",
    )
    generate_recipe.add_argument(
        "--no-fix-deps",
        action="store_true",
        help="Do NOT auto-repair pins that violate a sibling package's declared "
        "constraints (checked against PyPI metadata before build). Conflicts are "
        "still reported.",
    )
    _add_venv_flag(generate_recipe)

    llm_recipe = subparsers.add_parser(
        "generate-recipe-from-llm",
        help="Use an LLM to draft a recipe for any model, then validate + render it.",
    )
    llm_source = llm_recipe.add_mutually_exclusive_group(required=True)
    llm_source.add_argument("--card", type=Path, help="Model-card JSON file (meta/readme/files).")
    llm_source.add_argument("--repo", help="Hugging Face model repo id to fetch the card from (network).")
    llm_source.add_argument(
        "--github",
        default="",
        help="GitHub repo URL/spec (owner/repo, full URL, or .../tree/<ref>/...). The "
        "repo's README seeds the card and its source files ground the LLM; the wrapper "
        "adds the repo as a git+ pip dependency. Set GITHUB_TOKEN to raise the API rate limit.",
    )
    llm_recipe.add_argument(
        "--ref",
        default="",
        help="Git branch/tag/SHA for --github (default: the repo's default branch).",
    )
    llm_recipe.add_argument(
        "--space",
        default="",
        help="Ground the LLM on an existing Space's source (its app.py + local modules). "
        "Strongly recommended for app-like Spaces so the wrapper reuses the real API/UI.",
    )
    llm_recipe.add_argument(
        "--remote-space",
        default="",
        help="Emit a REMOTE-BACKEND (proxy) wrapper that calls this existing Gradio "
        "Space via gradio_client instead of installing the model. Best for models "
        "that aren't pip-installable or whose deps conflict with pyharp (e.g. "
        "SonicMaster, SoulX-Singer). The recipe is scaffolded deterministically from "
        "the Space's live API (no LLM guessing); the model card is enriched from "
        "--card/--repo/--github. Frontend deps become just pyharp + gradio + "
        "gradio_client -- conflict-free by construction.",
    )
    llm_recipe.add_argument(
        "--remote-api-name",
        default="",
        help="With --remote-space: the backend named endpoint to call (e.g. "
        "/enhance_audio_ui). Required only if the Space exposes more than one.",
    )
    llm_recipe.add_argument(
        "--user-token",
        action="store_true",
        help="With --remote-space: add an optional masked 'Hugging Face token' control "
        "to the frontend so ZeroGPU usage on the backend is attributed to the calling "
        "user's account (fallback: this Space's HF_TOKEN secret).",
    )
    llm_recipe.add_argument(
        "--remote-llm",
        action="store_true",
        help="With --remote-space: let the LLM REFINE the deterministic scaffold "
        "(decide which args to expose vs. send as constants, dropdown choices, "
        "slider ranges, labels, model card), grounded on the live API schema. The "
        "backend space/api_name and the args count/order stay pinned, so the call "
        "signature can't drift. Without this flag the scaffold is used verbatim.",
    )
    llm_recipe.add_argument(
        "--backend",
        action="store_true",
        help="Generate a plain-Gradio BACKEND recipe (no pyharp) that RUNS the model and "
        "exposes /predict, for the two-Space workflow. Best for a GitHub model with no "
        "existing Space (e.g. magenta/ddsp): deploy this backend, then point a "
        "remote-backend frontend at it. Mutually exclusive with --remote-space.",
    )
    llm_recipe.add_argument(
        "--dual",
        action="store_true",
        help="Generate a dual-interpreter Docker recipe: one pyHARP frontend plus an "
        "isolated backend worker venv/interpreter. Best for models whose dependencies "
        "conflict with pyHARP/Gradio but can still be installed in a separate backend "
        "environment. Mutually exclusive with --backend and --remote-space.",
    )
    llm_recipe.add_argument(
        "--inputs", default="", help="Comma-separated desired input types (e.g. audio,slider)."
    )
    llm_recipe.add_argument(
        "--outputs", default="", help="Comma-separated desired output types (e.g. audio,labels)."
    )
    llm_recipe.add_argument(
        "--provider",
        choices=["gemini"],
        help="LLM provider (default: auto-detect from API key env vars).",
    )
    llm_recipe.add_argument(
        "--llm-model", default=None, help="Provider model name (default: provider's default)."
    )
    llm_recipe.add_argument(
        "--llm-timeout", type=float, default=120.0, help="LLM API timeout in seconds."
    )
    llm_recipe.add_argument(
        "--temperature", type=float, default=0.2, help="LLM sampling temperature."
    )
    llm_recipe.add_argument(
        "--max-repairs", type=int, default=2, help="Max validate/repair iterations."
    )
    llm_recipe.add_argument(
        "--no-examples",
        action="store_true",
        help="Do not include corpus few-shot examples in the prompt.",
    )
    llm_recipe.add_argument(
        "--emit-anyway",
        action="store_true",
        help="With --github: write the drafted recipe even when the deterministic "
        "classifier judges the repo won't deploy as a single self-contained Space "
        "(native-fragile deps, gradio/python conflicts, or not pip-installable). By "
        "default such a doomed draft is NOT written; the tool prints the recommended "
        "fallback mode instead.",
    )
    llm_recipe.add_argument("--output", type=Path, help="Optional recipe JSON output path.")
    llm_recipe.add_argument(
        "--generate-package",
        action="store_true",
        help="Also render and write the wrapper package from the drafted recipe.",
    )
    llm_recipe.add_argument(
        "--package-output",
        type=Path,
        default=Path("artifacts/model_agent/generated"),
        help="Package output directory for --generate-package/--smoke-test.",
    )
    llm_recipe.add_argument(
        "--smoke-test",
        action="store_true",
        help="Build the package and smoke-test it (implies --generate-package; runs downloaded code).",
    )
    llm_recipe.add_argument(
        "--no-fix-deps",
        action="store_true",
        help="Do NOT auto-repair pins that violate a sibling package's declared "
        "constraints (checked against PyPI metadata). Conflicts are still reported.",
    )
    _add_venv_flag(llm_recipe)

    list_models = subparsers.add_parser(
        "list-models",
        help="List LLM models that support content generation for the configured provider.",
    )
    list_models.add_argument(
        "--provider",
        choices=["gemini"],
        help="LLM provider (default: auto-detect from API key env vars).",
    )
    list_models.add_argument("--llm-timeout", type=float, default=60.0, help="API timeout (s).")

    complete = subparsers.add_parser(
        "complete-recipe",
        help="Use an LLM to fill the _todo stubs of a scaffolded recipe (preserves I/O).",
    )
    complete.add_argument("recipe", type=Path, help="Partial recipe JSON with _todo stubs to fill.")
    complete.add_argument(
        "--card", type=Path, help="Optional model-card JSON to enrich the prompt with the README."
    )
    complete.add_argument(
        "--repo", help="Optional Hugging Face repo id to fetch the card from (network)."
    )
    complete.add_argument(
        "--provider",
        choices=["gemini"],
        help="LLM provider (default: auto-detect from API key env vars).",
    )
    complete.add_argument("--llm-model", default=None, help="Provider model name.")
    complete.add_argument("--llm-timeout", type=float, default=120.0, help="LLM API timeout (s).")
    complete.add_argument("--temperature", type=float, default=0.2, help="LLM sampling temperature.")
    complete.add_argument("--max-repairs", type=int, default=2, help="Max validate/repair iterations.")
    complete.add_argument("--output", type=Path, help="Optional completed recipe JSON output path.")
    complete.add_argument(
        "--generate-package",
        action="store_true",
        help="Also render and write the wrapper package from the completed recipe.",
    )
    complete.add_argument(
        "--package-output",
        type=Path,
        default=Path("artifacts/model_agent/generated"),
        help="Package output directory for --generate-package/--smoke-test.",
    )
    complete.add_argument(
        "--smoke-test",
        action="store_true",
        help="Build the package and smoke-test it (implies --generate-package; runs downloaded code).",
    )
    _add_venv_flag(complete)

    smoke = subparsers.add_parser(
        "smoke-test",
        help="Launch a generated package's app.py and verify it exposes HARP controls.",
    )
    smoke.add_argument("package", type=Path, help="Path to a generated package folder.")
    smoke.add_argument(
        "--python",
        dest="python_executable",
        default=None,
        help="Python interpreter to run app.py with (defaults to the current one).",
    )
    smoke.add_argument(
        "--startup-timeout",
        type=float,
        default=180.0,
        help="Seconds to wait for the Gradio URL before failing.",
    )
    _add_venv_flag(smoke)

    deploy = subparsers.add_parser(
        "deploy-space",
        help="Push a generated package to a Hugging Face Space (needs huggingface_hub + a write token).",
    )
    deploy.add_argument("package", type=Path, help="Path to a generated package folder.")
    deploy.add_argument(
        "--repo",
        required=True,
        help="Target Space id, e.g. your-username/your-space-name.",
    )
    deploy.add_argument(
        "--token",
        default=None,
        help="HF write token (defaults to HF_TOKEN / HUGGING_FACE_HUB_TOKEN env, or cached login).",
    )
    deploy.add_argument(
        "--private",
        action="store_true",
        help="Create the Space as private.",
    )
    deploy.add_argument(
        "--sdk",
        default="gradio",
        help="Space SDK (default: gradio). Use 'docker' for dual-interpreter bundles.",
    )
    deploy.add_argument(
        "--into-space",
        action="store_true",
        help="Overlay app.py onto an EXISTING Space (e.g. a duplicate of the model's "
        "original Space), reconciling gradio/sdk_version with pyharp and preserving "
        "the Space's own code. Use this for wrappers that import the Space's modules.",
    )
    deploy.add_argument(
        "--gradio-version",
        default=HARP_GRADIO_VERSION,
        help=f"Gradio version to reconcile to with --into-space (default: {HARP_GRADIO_VERSION}, "
        "pyharp's pin).",
    )
    deploy.add_argument(
        "--freeze-from",
        default=None,
        help="With --into-space: path to a known-good `pip freeze` file. Locks the "
        "Space's requirements to that exact closure (re-pinning declared deps and "
        "common audio/text ML libs) so they can't drift when gradio is forced down "
        "-- the usual cause of correct melody but gibberish words.",
    )
    deploy.add_argument(
        "--message",
        default="Deploy HARP wrapper via model agent",
        help="Commit message for the upload.",
    )
    deploy.add_argument(
        "--no-record",
        action="store_true",
        help="Do not record this deployment in the knowledge base on success.",
    )

    recommend = subparsers.add_parser(
        "recommend-mode",
        help="Analyze a repo and recommend a deployment mode (single / remote / "
        "dual / two-space) from deterministic signals, plus similar past deployments.",
    )
    rec_source = recommend.add_mutually_exclusive_group(required=True)
    rec_source.add_argument("--github", default="", help="GitHub repo URL/spec (owner/repo or URL).")
    rec_source.add_argument("--card", type=Path, help="Model-card JSON file (meta/readme/files).")
    rec_source.add_argument("--repo", help="Hugging Face model repo id to fetch the card from.")
    recommend.add_argument("--ref", default="", help="Git branch/tag/SHA for --github.")
    recommend.add_argument(
        "--existing-space",
        default="",
        help="An existing backend Space id that could serve this model (enables the "
        "remote fallback). Pass the Space you'd proxy to.",
    )
    recommend.add_argument("--output", type=Path, help="Optional JSON output path.")

    knowledge = subparsers.add_parser(
        "knowledge",
        help="Query the deployment knowledge base (past deployments, similar stacks, "
        "and error->fix repair rules).",
    )
    knowledge.add_argument("--find-repo", default="", help="List past deployments for a repo id.")
    knowledge.add_argument(
        "--similar",
        type=Path,
        help="A recipe JSON: list past deployments with the same dependency fingerprint.",
    )
    knowledge.add_argument(
        "--diagnose",
        default="",
        help="An error message/log: return matching repair rules and past failure cases.",
    )
    knowledge.add_argument("--output", type=Path, help="Optional JSON output path.")

    for _name, _alias in (("deploy", False), ("port", True)):
        _dep = subparsers.add_parser(
            _name,
            help=(
                "One command to deploy any model: detect the ref (GitHub repo / HF "
                "model / HF Space), analyze it, pick the deployment mode, and run the "
                "right steps. Prints the plan and asks before deploying (use --yes to "
                "run unattended, --plan to only show the plan)."
                + (" Alias of `deploy`." if _alias else "")
            ),
        )
        _dep.add_argument(
            "ref",
            nargs="?",
            default="",
            help="Model reference: a GitHub URL/owner-repo, an HF model id, or an HF "
            "Space URL/id (e.g. https://github.com/haoheliu/voicefixer, "
            "facebook/MusicGen, Soul-AILab/SoulX-Singer). If omitted, you'll be "
            "prompted for it.",
        )
        _dep.add_argument(
            "--repo",
            default="",
            help="Target HF Space to deploy to. Either the full 'owner/name', or just "
            "your org/owner (e.g. 'teamup-tech'), in which case the Space name is taken "
            "from the model reference (so '.../pedalboard --repo teamup-tech' targets "
            "'teamup-tech/pedalboard'). Prompted for when omitted; required unless --plan.",
        )
        _dep.add_argument(
            "--source",
            choices=["auto", "github", "hf-model", "hf-space"],
            default="auto",
            help="Force how to interpret a bare owner/name ref (default: auto -> HF model).",
        )
        _dep.add_argument(
            "--space",
            default="",
            help="Proxy to THIS backend Space instead of auto-discovering one "
            "(owner/space). Forces remote mode.",
        )
        _dep.add_argument("--ref-rev", default="", help="Git branch/tag/SHA for a GitHub ref.")
        _dep.add_argument(
            "--plan",
            action="store_true",
            help="Only analyze and print the plan + the exact commands; never deploy.",
        )
        _dep.add_argument(
            "--yes",
            "-y",
            action="store_true",
            help="Deploy without the interactive confirmation prompt (unattended).",
        )
        _dep.add_argument(
            "--user-token",
            action=argparse.BooleanOptionalAction,
            default=True,
            help="For remote/proxy deploys, include the optional masked HF-token field "
            "so callers can paste their own token (ZeroGPU billed to them; default: on). "
            "Use --no-user-token to omit it and rely only on a Space HF_TOKEN secret.",
        )
        _dep.add_argument(
            "--no-discover-space",
            action="store_true",
            help="Don't auto-discover an existing backend Space; build from source instead.",
        )
        _dep.add_argument(
            "--mode",
            choices=["auto", "dual", "backend"],
            default="auto",
            help="When the model needs isolation (can't share a process with pyharp), "
            "choose 'dual' (one Docker Space: pyharp + isolated backend) or 'backend' "
            "(plain Gradio backend only; later re-run with --space for the HARP "
            "frontend). Default 'auto' asks interactively when both are available, "
            "or picks the classifier default under --yes/--plan.",
        )
        _dep.add_argument("--api-name", default="", help="Backend endpoint to call (remote mode).")
        _dep.add_argument("--inputs", default="", help="Desired input types for single-Space mode.")
        _dep.add_argument("--outputs", default="", help="Desired output types for single-Space mode.")
        _dep.add_argument(
            "--recipe-output", type=Path, default=None, help="Where to write the recipe JSON."
        )
        _dep.add_argument(
            "--package-output",
            type=Path,
            default=Path("artifacts/model_agent/generated"),
            help="Parent directory for the generated package.",
        )
        _dep.add_argument("--output", type=Path, help="Optional JSON output path for the plan.")

    return parser


def main(argv: Iterable[str] | None = None) -> int:
    args = build_parser().parse_args(list(argv) if argv is not None else None)
    agent = HarpModelAgent()
    agent.scraper.timeout = args.timeout
    agent.endpoint_client.timeout = args.timeout

    try:
        if args.command == "discover":
            candidates = agent.scraper.discover(
                query=args.query,
                author=args.author,
                tags=args.tag,
                limit=args.limit,
            )
            if not args.all:
                candidates = [
                    candidate
                    for candidate in candidates
                    if candidate.looks_open_source() and candidate.looks_gradio()
                ]
            payload = [candidate.__dict__ for candidate in candidates]
            _emit_json(payload, args.output)
            return 0

        if args.command == "scaffold-remote-recipe":
            print(
                f"Probing the backend Space's Gradio API: {args.space} ...",
                file=sys.stderr,
            )
            recipe = agent.scaffold_remote_recipe(
                args.space,
                api_name=args.api_name or None,
                user_token=getattr(args, "user_token", False),
                auto_endpoint=getattr(args, "auto_endpoint", False),
            )
            remote = recipe.get("framework", {}).get("remote", {})
            print(
                f"  Scaffolded remote recipe for endpoint '{remote.get('api_name')}' "
                f"({len(recipe.get('inputs', []))} input(s), "
                f"{len(recipe.get('outputs', []))} output(s)). Review the _todo notes "
                "before rendering/deploying.",
                file=sys.stderr,
            )
            _emit_json(recipe, args.output)
            return 0

        if args.command == "render-recipe":
            app_py = render_app_from_recipe(_read_json(args.recipe))
            _warn_app_lint(app_py)
            if args.output:
                args.output.parent.mkdir(parents=True, exist_ok=True)
                args.output.write_text(app_py, encoding="utf-8")
            print(app_py)
            return 0

        if args.command == "generate-recipe":
            recipe = _read_json(args.recipe)
            if getattr(args, "backend", False) and isinstance(recipe, dict):
                framework = recipe.get("framework")
                if not isinstance(framework, dict):
                    framework = {}
                    recipe["framework"] = framework
                framework["backend"] = True
            _warn_recipe_requirements(recipe)
            if not getattr(args, "no_fix_deps", False):
                _resolve_dependency_conflicts(recipe, agent, fix=True)
            package = build_package_from_recipe(recipe)
            _warn_app_lint(package.app_py)
            folder = agent.write_generated_app_package(package, args.output)
            result = {"package": str(folder), "framework": package.framework}
            if args.smoke_test:
                result["smoke_test"] = _run_smoke_test(
                    agent, folder, use_venv=getattr(args, "venv", False)
                ).to_json()
            _emit_json(result, None)
            return 0 if result.get("smoke_test", {}).get("ok", True) else 4

        if args.command == "generate-recipe-from-llm":
            if getattr(args, "backend", False) and args.remote_space:
                print(
                    "error: --backend and --remote-space are mutually exclusive. Use "
                    "--remote-space to PROXY an existing Space, or --backend to build a "
                    "new plain-Gradio backend that runs the model.",
                    file=sys.stderr,
                )
                return 2
            if getattr(args, "dual", False) and args.remote_space:
                print(
                    "error: --dual and --remote-space are mutually exclusive. Use "
                    "--remote-space to PROXY an existing Space, or --dual to build one "
                    "Docker Space with isolated frontend/backend interpreters.",
                    file=sys.stderr,
                )
                return 2
            if getattr(args, "dual", False) and getattr(args, "backend", False):
                print(
                    "error: --dual and --backend are mutually exclusive. Use --dual "
                    "for one Docker Space, or --backend for the model-running half of "
                    "a two-Space deployment.",
                    file=sys.stderr,
                )
                return 2
            github_target = None
            if args.github:
                github_target = agent.resolve_github_target(args.github, ref=args.ref or None)
                owner, repo, ref = github_target
                print(
                    f"Reading model card from GitHub repo: {owner}/{repo}@{ref} ...",
                    file=sys.stderr,
                )
                card = agent.get_github_card(args.github, ref=ref)
            elif args.card:
                card = _read_json(args.card)
            else:
                card = _fetch_card_or_exit(agent, args.repo)

            # Remote-backend (proxy) mode: skip the LLM entirely and scaffold a
            # thin gradio_client wrapper deterministically from the Space's live
            # API, enriching only the model card from the fetched metadata. This
            # is the right path for models that aren't pip-installable or whose
            # deps conflict with pyharp -- exactly the "generate something simple"
            # case that installing a non-package git+ repo cannot satisfy.
            if args.remote_space:
                print(
                    f"Remote-backend mode: probing the live API of {args.remote_space} ...",
                    file=sys.stderr,
                )
                api_info = agent.fetch_api_info(args.remote_space)
                canonical = (
                    agent.endpoint_client.resolve_canonical_path(args.remote_space)
                    or args.remote_space
                )
                # If the user didn't pin --remote-api-name and the Space exposes
                # several endpoints, choose one instead of erroring: the LLM picks
                # (with --remote-llm), otherwise a deterministic heuristic does.
                selected_api = args.remote_api_name or None
                named = api_info.get("named_endpoints") or {}
                provider = None
                if args.remote_llm:
                    provider = provider_from_env(
                        args.provider,
                        model=args.llm_model,
                        timeout=args.llm_timeout,
                        temperature=args.temperature,
                    )
                if not selected_api and isinstance(named, Mapping) and len(named) > 1:
                    if provider is not None:
                        selected_api = pick_remote_endpoint(
                            api_info, provider, context=RecipeGenerationContext.from_card(card)
                        )
                        print(
                            f"  LLM selected endpoint '{selected_api}' from "
                            f"{sorted(named)}.",
                            file=sys.stderr,
                        )
                    else:
                        selected_api = guess_primary_endpoint(api_info)
                        print(
                            f"  Auto-selected endpoint '{selected_api}' (heuristic) from "
                            f"{sorted(named)}; pass --remote-api-name to override, or "
                            "--remote-llm to let the LLM choose.",
                            file=sys.stderr,
                        )
                scaffold = remote_recipe_from_api_info(
                    canonical,
                    api_info,
                    api_name=selected_api,
                    user_token=getattr(args, "user_token", False),
                )
                if args.remote_llm:
                    chosen = str(scaffold["framework"]["remote"]["api_name"])
                    endpoint = (api_info.get("named_endpoints") or {}).get(chosen, {})
                    print(
                        f"  Refining the scaffold with the LLM (grounded on '{chosen}'; "
                        "space/api_name and arg order stay pinned) ...",
                        file=sys.stderr,
                    )
                    llm_context = RecipeGenerationContext.from_card(card)
                    # Ground the refinement on the backend Space's own UI source so
                    # the LLM can fill REAL dropdown choices and tell which optional/
                    # internal args (e.g. metadata slots) to send as constants rather
                    # than expose. The /info schema alone lacks both. Default to the
                    # backend Space; --space can override the grounding source.
                    source_space = args.space or args.remote_space
                    print(
                        f"  Grounding the refinement on the Space UI source: {source_space} ...",
                        file=sys.stderr,
                    )
                    llm_context.space_sources = agent.fetch_space_sources(source_space)
                    if not llm_context.space_sources:
                        print(
                            "  (no readable Space source found; refining from the "
                            "/info schema + README only -- dropdown choices and "
                            "hidden-arg decisions may need manual review)",
                            file=sys.stderr,
                        )
                    draft = refine_remote_recipe(
                        scaffold,
                        endpoint,
                        provider,
                        context=llm_context,
                        max_repairs=args.max_repairs,
                    )
                else:
                    print(
                        "  Using the deterministic scaffold verbatim (pass --remote-llm "
                        "to have the LLM refine it).",
                        file=sys.stderr,
                    )
                    _apply_card_metadata(scaffold, card)
                    draft = RecipeDraft(
                        recipe=scaffold,
                        app_py=render_app_from_recipe(scaffold),
                        attempts=0,
                        provider="scaffold",
                        model="remote-backend",
                    )
                return _emit_recipe_draft(agent, draft, args)

            context = RecipeGenerationContext.from_card(
                card,
                target_inputs=_split_csv(args.inputs),
                target_outputs=_split_csv(args.outputs),
                examples=[] if args.no_examples else default_examples(dual=getattr(args, "dual", False)),
            )
            if github_target is not None:
                owner, repo, ref = github_target
                print(
                    f"Grounding on the GitHub source: {owner}/{repo}@{ref} ...",
                    file=sys.stderr,
                )
                context.space_sources = agent.fetch_github_sources(args.github, ref=ref)
                context.grounding_origin = "github"
                context.source_repo_url = f"git+https://github.com/{owner}/{repo}.git@{ref}"
                if not context.space_sources:
                    print(
                        "  (no readable .py source found in the repo; "
                        "falling back to the README only)",
                        file=sys.stderr,
                    )
                # Ground framework.pip on the repo's declared dependency versions
                # (setup.py install_requires / requirements.txt / pyproject) so pins
                # come from the source of truth instead of the LLM's guess.
                context.dependency_manifests = agent.fetch_github_dependencies(
                    args.github, ref=ref
                )
                if context.dependency_manifests:
                    print(
                        "  Grounding dependencies on declared manifests: "
                        + ", ".join(sorted(context.dependency_manifests)),
                        file=sys.stderr,
                    )
                context.repo_pip_installable = _pip_installable_from_signals(
                    card, context.dependency_manifests or {}
                )
                if not context.repo_pip_installable:
                    print(
                        "  Repo is not pip-installable (no root packaging file); "
                        "backend recipes will inline inference from the sources.",
                        file=sys.stderr,
                    )
            elif args.space:
                print(
                    f"Grounding on the original Space source: {args.space} ...",
                    file=sys.stderr,
                )
                context.space_sources = agent.fetch_space_sources(args.space)
                if not context.space_sources:
                    print(
                        f"  (no readable source found for Space '{args.space}'; "
                        "falling back to the model card only)",
                        file=sys.stderr,
                    )
            provider = provider_from_env(
                args.provider,
                model=args.llm_model,
                timeout=args.llm_timeout,
                temperature=args.temperature,
            )
            draft = generate_recipe(
                context,
                provider,
                max_repairs=args.max_repairs,
                backend=getattr(args, "backend", False),
                dual=getattr(args, "dual", False),
            )
            if github_target is not None:
                owner, repo, _ref = github_target
                manifests = context.dependency_manifests or {}
                # Authoritative pip-installability: a root packaging file fetched by
                # the dependency grounding proves installability even when the card's
                # (often partial) file listing misses it. Fall back to the card only
                # when no packaging manifest was fetched.
                if _pip_installable_from_signals(card, manifests):
                    is_dual = getattr(args, "dual", False)
                    # Prefer the repo's published PyPI wheel over `git+<repo>`:
                    # a git+ install rebuilds from source, which fails for native
                    # / submodule repos (e.g. pedalboard pulls pybind11 over SSH).
                    pypi_name = None
                    try:
                        pypi_name = agent.resolve_pypi_distribution(manifests, repo)
                    except Exception:  # noqa: BLE001 - PyPI lookup is best-effort
                        pypi_name = None
                    if pypi_name:
                        print(
                            f"  {owner}/{repo} publishes '{pypi_name}' on PyPI -> "
                            "installing the wheel instead of git+ (avoids source/"
                            "submodule builds).",
                            file=sys.stderr,
                        )
                        _strip_repo_pip(draft.recipe, context.source_repo_url)
                        _strip_repo_backend_pip(draft.recipe, context.source_repo_url)
                        _ensure_pip_distribution(draft.recipe, pypi_name, dual=is_dual)
                    elif is_dual:
                        _ensure_github_backend_pip(draft.recipe, context.source_repo_url)
                    else:
                        _ensure_github_pip(draft.recipe, context.source_repo_url)
                else:
                    # No setup.py/pyproject.toml -> `pip install git+<repo>` fails with
                    # "does not appear to be a Python project". Strip that doomed line
                    # (the LLM often adds it) and steer to remote-backend mode instead.
                    _strip_repo_pip(draft.recipe, context.source_repo_url)
                    _strip_repo_backend_pip(draft.recipe, context.source_repo_url)
                    if getattr(args, "dual", False) and not getattr(args, "emit_anyway", False):
                        print(
                            "error: this GitHub repo has no root setup.py/pyproject.toml/"
                            "setup.cfg, so the generated dual backend cannot install it "
                            "with a git+ dependency. Use a remote/two-space backend or "
                            "re-run with --emit-anyway to write the draft for manual repair.",
                            file=sys.stderr,
                        )
                        return 2
                    # Script-style backend: ship the repo's .py modules next to app.py
                    # so imports work without git+ (and without runtime urllib downloads).
                    if getattr(args, "backend", False) and context.space_sources:
                        n_vendor = attach_vendor_files(
                            draft.recipe, context.space_sources
                        )
                        if n_vendor:
                            print(
                                f"  Vendored {n_vendor} source file(s) into the backend "
                                "package (framework.vendor_files).",
                                file=sys.stderr,
                            )
                # Single-Space feasibility gate: don't hand back a recipe that the
                # deterministic classifier says can't deploy as one self-contained
                # pyharp Space (unless the user asked for a backend or --emit-anyway).
                if not getattr(args, "backend", False) and not getattr(args, "dual", False):
                    decision = recommend_mode(
                        manifests=manifests,
                        sources=context.space_sources or {},
                        card=card if isinstance(card, dict) else None,
                    )
                    if decision.mode != "single" and not getattr(args, "emit_anyway", False):
                        _print_single_space_gate(owner, repo, decision)
                        return 2
            if getattr(args, "backend", False) and isinstance(draft.recipe, dict):
                ensure_backend_runtime_defaults(draft.recipe)
                py_ver = (draft.recipe.get("framework") or {}).get("python_version")
                if py_ver:
                    print(
                        f"  Backend Space python_version={py_ver} "
                        "(needed for legacy TensorFlow / research pins).",
                        file=sys.stderr,
                    )
            _print_resource_headsup(card, context)
            return _emit_recipe_draft(agent, draft, args)

        if args.command == "list-models":
            provider = provider_from_env(args.provider, timeout=args.llm_timeout)
            _emit_json({"provider": provider.name, "models": provider.list_models()}, None)
            return 0

        if args.command == "complete-recipe":
            base = _read_json(args.recipe)
            context = None
            if args.card:
                context = RecipeGenerationContext.from_card(_read_json(args.card))
            elif args.repo:
                context = RecipeGenerationContext.from_card(_fetch_card_or_exit(agent, args.repo))
            provider = provider_from_env(
                args.provider,
                model=args.llm_model,
                timeout=args.llm_timeout,
                temperature=args.temperature,
            )
            draft = complete_recipe(
                base, provider, context=context, max_repairs=args.max_repairs
            )
            return _emit_recipe_draft(agent, draft, args)

        if args.command == "smoke-test":
            result = _run_smoke_test(
                agent,
                args.package,
                python_executable=args.python_executable,
                startup_timeout_s=args.startup_timeout,
                use_venv=args.venv,
            )
            _emit_json(result.to_json(), None)
            return 0 if result.ok else 4

        if args.command == "deploy-space":
            log = lambda message: print(f"  [deploy] {message}", file=sys.stderr)
            if args.into_space:
                print(
                    f"Overlaying HARP wrapper onto existing Space {args.repo} "
                    "(reconciling dependencies, preserving its code)...",
                    file=sys.stderr,
                )
                result = agent.deploy_into_space(
                    args.package,
                    args.repo,
                    token=args.token,
                    gradio_version=args.gradio_version,
                    freeze_from=args.freeze_from,
                    commit_message=args.message,
                    log=log,
                )
            else:
                if args.freeze_from:
                    print(
                        "  [deploy] note: --freeze-from only applies with --into-space; ignoring.",
                        file=sys.stderr,
                    )
                print(
                    "Deploying to a Hugging Face Space (creates/updates a remote repo "
                    "under your account)...",
                    file=sys.stderr,
                )
                result = agent.deploy_space(
                    args.package,
                    args.repo,
                    token=args.token,
                    private=args.private,
                    space_sdk=args.sdk,
                    commit_message=args.message,
                    log=log,
                )
            if not getattr(args, "no_record", False):
                _record_deployment(args.package, result, log=log)
            _emit_json(result, None)
            return 0

        if args.command == "recommend-mode":
            decision = _recommend_mode(agent, args)
            _emit_json(decision, args.output)
            return 0

        if args.command == "knowledge":
            _emit_json(_knowledge_query(args), args.output)
            return 0

        if args.command in ("deploy", "port"):
            return _run_deploy(agent, args)

    except DeploySpaceError as exc:
        print(f"Space deploy failed: {exc}", file=sys.stderr)
        return 2
    except EndpointProbeError as exc:
        print(f"Endpoint probe failed: {exc}", file=sys.stderr)
        return 2
    except RecipeError as exc:
        print(f"Recipe invalid: {exc}", file=sys.stderr)
        return 2
    except LLMError as exc:
        print(f"LLM recipe generation failed: {exc}", file=sys.stderr)
        return 2
    except ValueError as exc:
        print(f"Packaging failed: {exc}", file=sys.stderr)
        return 2
    except (HTTPError, URLError, OSError) as exc:
        print(f"Network request failed: {exc}", file=sys.stderr)
        return 2

    return 1


def _fetch_card_or_exit(agent: HarpModelAgent, repo: str) -> object:
    """Fetch a Hugging Face model card, with a clear message for auth failures."""

    try:
        card = agent.scraper.get_model_card(repo)
    except HTTPError as exc:
        if exc.code in (401, 403):
            raise SystemExit(
                f"Could not fetch the model card for '{repo}' (HTTP {exc.code}). "
                "Hugging Face returns this for private/gated/nonexistent model repos "
                "(e.g. libraries like Demucs that are not a HF model card). Use a public "
                "model repo id, or pass a local card with --card <file.json>."
            )
        raise
    if card is None:
        raise SystemExit(f"Hugging Face model repo not found: {repo}")
    return card


def _warn_app_lint(app_py: str) -> None:
    """Print heuristic warnings about a generated app.py to stderr."""

    for warning in lint_generated_app(app_py):
        print(f"  [lint] WARNING: {warning}", file=sys.stderr)


def _warn_recipe_requirements(recipe: dict) -> None:
    """Print heuristic warnings about a recipe's pip dependencies to stderr."""

    for warning in lint_recipe_requirements(recipe):
        print(f"  [deps] WARNING: {warning}", file=sys.stderr)


def _resolve_dependency_conflicts(recipe: dict, agent: HarpModelAgent, *, fix: bool = True) -> None:
    """Detect (and by default auto-repair) pins that violate a sibling package's
    declared constraints, using PyPI metadata. Best-effort: silently no-ops when
    offline. This is what turns 'librosa==0.10.1 vs ddsp needs librosa<=0.10'
    into an up-front, auto-fixed problem instead of a failed 10-minute build.
    """

    requirements = collect_pip_requirements(recipe)
    if not requirements:
        return
    try:
        conflicts = find_dependency_conflicts(
            requirements,
            requires_dist_of=agent.pypi_requires_dist,
            available_versions=agent.pypi_available_versions,
        )
    except Exception:
        return

    fixes: dict = {}
    for conflict in conflicts:
        sources = "; ".join(
            f"{src} requires {conflict['package']}{spec}"
            for src, spec in conflict["violations"]
        )
        message = (
            f"{conflict['package']}=={conflict['pinned']} conflicts with declared "
            f"constraints ({sources})."
        )
        if conflict["suggestion"]:
            message += f" Newest compatible version: {conflict['package']}=={conflict['suggestion']}."
            if fix:
                fixes[conflict["package"]] = conflict["suggestion"]
        else:
            message += f" Pin a version satisfying: {conflict['combined']}."
        print(f"  [deps] CONFLICT: {message}", file=sys.stderr)

    if fix and fixes:
        apply_dependency_fixes(recipe, fixes)
        applied = ", ".join(f"{name}=={version}" for name, version in fixes.items())
        print(
            f"  [deps] auto-repaired conflicting pins: {applied} "
            "(the package built here is fixed; the source recipe file is unchanged)",
            file=sys.stderr,
        )


def _recommend_mode(agent: HarpModelAgent, args) -> dict:
    """Analyze a repo and recommend a deployment mode, with similar past deploys."""

    manifests: dict = {}
    sources: dict = {}
    card = None
    repo_key = ""
    if args.github:
        owner, repo, ref = agent.resolve_github_target(args.github, ref=args.ref or None)
        repo_key = f"{owner}/{repo}"
        print(f"Analyzing GitHub repo {owner}/{repo}@{ref} ...", file=sys.stderr)
        card = agent.get_github_card(args.github, ref=ref)
        manifests = agent.fetch_github_dependencies(args.github, ref=ref)
        sources = agent.fetch_github_sources(args.github, ref=ref)
    elif args.card:
        card = _read_json(args.card)
        meta = card.get("meta") if isinstance(card, dict) else {}
        repo_key = str((meta or {}).get("id") or "")
    else:
        card = _fetch_card_or_exit(agent, args.repo)
        repo_key = str(args.repo)

    decision = recommend_mode(
        manifests=manifests,
        sources=sources,
        card=card if isinstance(card, dict) else None,
        has_existing_space=bool(args.existing_space),
    )
    payload = decision.to_dict()
    payload["repo"] = repo_key

    readme = str(card.get("readme") or "") if isinstance(card, dict) else ""
    resource_blob = "\n".join([readme] + list(manifests.values()) + list(sources.values()))
    warnings = detect_resource_warnings(resource_blob)
    if warnings.get("largest_size_gb") or warnings.get("gpu_evidence"):
        payload["resource_warnings"] = warnings
        headsup = resource_headsup(warnings)
        if headsup:
            payload.setdefault("recommendations", []).append(headsup)

    kb = KnowledgeBase()
    similar = kb.find_similar(
        repo=repo_key,
        requirements=list(manifests.get("requirements.txt", "").splitlines()) or None,
    )
    if similar:
        payload["similar_past_deployments"] = [
            {k: rec.get(k) for k in ("repo", "mode", "python", "outcome", "notes")}
            for rec in similar
        ]
    return payload


def _discover_backend_space(
    agent: HarpModelAgent, target, blob: str
) -> Optional[str]:
    """Best-effort: find a runnable Space linked from the model's README/source.

    We only accept a candidate whose live Gradio API actually responds, so a
    stale/dead link never routes us into remote mode.
    """

    for candidate in extract_space_links(blob):
        if candidate.lower() == target.slug.lower() and target.kind != "hf_space":
            continue
        try:
            agent.fetch_api_info(candidate)
        except Exception:
            continue
        return candidate
    return None


def _print_plan(target, plan, steps) -> None:
    """Human-readable plan preview (to stderr, so JSON stays clean on stdout)."""

    print("\n=== Deployment plan ===", file=sys.stderr)
    print(f"  model:   {target.slug}  ({target.kind})", file=sys.stderr)
    print(f"  mode:    {plan.mode}", file=sys.stderr)
    if plan.backend_space:
        print(f"  backend: {plan.backend_space}", file=sys.stderr)
    if plan.choices:
        print(f"  choices: {', '.join(plan.choices)}", file=sys.stderr)
    for reason in plan.rationale:
        print(f"  why:     {reason}", file=sys.stderr)
    for blocker in plan.blockers:
        print(f"  blocker: {blocker}", file=sys.stderr)
    for hint in plan.guidance:
        print(f"  next:    {hint}", file=sys.stderr)
    if steps:
        print("\n  commands the agent will run:", file=sys.stderr)
        for step in steps:
            print(f"    python -m tools.model_agent {' '.join(step)}", file=sys.stderr)
    print("", file=sys.stderr)


def _prompt_isolation_mode(choices: List[str], *, default: str) -> str:
    """Ask the user to pick dual vs backend when both isolation paths are open."""

    labels = {
        "dual": "dual     — one Docker Space (pyharp frontend + isolated model backend)",
        "backend": "backend  — plain Gradio backend only (Phase 1); HARP frontend later via --space",
    }
    print("\nThis model needs isolation from pyharp. Choose an approach:", file=sys.stderr)
    for choice in choices:
        marker = " (default)" if choice == default else ""
        print(f"  [{choice[0]}] {labels.get(choice, choice)}{marker}", file=sys.stderr)
    prompt = f"Approach [{'/'.join(c[0] for c in choices)}]"
    if default:
        prompt += f" (Enter={default[0]})"
    prompt += ": "
    try:
        reply = input(prompt).strip().lower()
    except EOFError:
        reply = ""
    if not reply:
        return default
    for choice in choices:
        if reply in (choice, choice[0]):
            return choice
    print(
        f"  Unrecognized choice {reply!r}; using {default!r}. "
        "Pass --mode dual|backend to skip this prompt.",
        file=sys.stderr,
    )
    return default


def _run_deploy(agent: HarpModelAgent, args) -> int:
    """One-command deploy: detect the ref, analyze it, pick a mode, and run it."""

    ref = (args.ref or "").strip()
    if not ref:
        # Widget-style: prompt for just the link when it wasn't passed.
        try:
            ref = input(
                "Model link (GitHub / HF model / HF Space URL or owner/name): "
            ).strip()
        except EOFError:
            ref = ""
        if not ref:
            print(
                "error: a model reference is required (a GitHub/HF URL or owner/name).",
                file=sys.stderr,
            )
            return 2
    args.ref = ref

    try:
        target = detect_ref(args.ref, args.source)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    # Allow an org-only --repo (e.g. "teamup-tech"): derive the Space name from
    # the reference so the user doesn't have to retype it. Prompt when missing
    # (unless we're only previewing the plan).
    if not args.repo and not args.plan:
        try:
            entered = input(
                f"Target Space [owner/name, or just an org -> owner/{target.name}]: "
            ).strip()
        except EOFError:
            entered = ""
        args.repo = entered
    args.repo = resolve_target_repo(args.repo, target)

    print(f"Analyzing {target.slug} (detected: {target.kind}) ...", file=sys.stderr)
    if args.repo:
        print(f"  Target Space: {args.repo}", file=sys.stderr)

    backend_space = args.space or ""
    decision = None
    card = None
    manifests: dict = {}
    sources: dict = {}

    if target.kind == "github":
        ref = args.ref_rev or None
        try:
            card = agent.get_github_card(target.slug, ref=ref)
            manifests = agent.fetch_github_dependencies(target.slug, ref=ref)
            sources = agent.fetch_github_sources(target.slug, ref=ref)
        except (HTTPError, URLError, OSError) as exc:
            print(f"  (could not fully read the GitHub repo: {exc})", file=sys.stderr)
    elif target.kind == "hf_model":
        if args.source == "auto":
            # A bare owner/name might actually be a Space, not a model. Try the
            # model card; if it's missing but the id serves a live Gradio API,
            # treat it as a Space and proxy to it.
            try:
                card = agent.scraper.get_model_card(target.slug)
            except (HTTPError, URLError, OSError):
                card = None
            if card is None:
                try:
                    agent.fetch_api_info(target.slug)
                    print(
                        f"  {target.slug} isn't a model card but serves a live API "
                        "-> treating it as a Space.",
                        file=sys.stderr,
                    )
                    target = RefTarget("hf_space", target.owner, target.name)
                except Exception:
                    card = _fetch_card_or_exit(agent, target.slug)  # helpful error/exit
        else:
            card = _fetch_card_or_exit(agent, target.slug)

    if target.kind == "hf_space":
        backend_space = backend_space or target.slug
    else:
        if not backend_space and not args.no_discover_space:
            readme = str(card.get("readme") or "") if isinstance(card, dict) else ""
            blob = "\n".join([readme] + list(sources.values()) + list(manifests.values()))
            found = _discover_backend_space(agent, target, blob)
            if found:
                backend_space = found
                print(f"  Discovered a runnable backend Space: {found}", file=sys.stderr)

    recipe_path = args.recipe_output or (
        Path("artifacts/model_agent/recipes") / f"{_slug(target.name)}.json"
    )
    package_parent = args.package_output

    if target.kind != "hf_space":
        decision = recommend_mode(
            manifests=manifests,
            sources=sources,
            card=card if isinstance(card, dict) else None,
            has_existing_space=bool(backend_space),
        )

    signals = (decision.signals if decision else {}) or {}
    pip_installable = bool(signals.get("pip_installable", True)) if decision else True
    python_floor = None
    floor_raw = signals.get("python_floor") if decision else None
    if isinstance(floor_raw, (list, tuple)) and len(floor_raw) == 2:
        try:
            python_floor = (int(floor_raw[0]), int(floor_raw[1]))
        except (TypeError, ValueError):
            python_floor = None
    # Script-style GitHub repos (REMI, etc.) have no setup.py but do have .py
    # sources we can ground a plain-Gradio backend on (LLM inlines inference).
    allow_source_backend = target.kind == "github" and (
        bool(sources)
        or any(
            str(name).endswith(".py")
            for name in ((card or {}).get("files") if isinstance(card, dict) else []) or []
        )
    )

    available = isolation_options(
        pip_installable=pip_installable,
        python_floor=python_floor,
        allow_source_backend=allow_source_backend,
    )
    # Ask (or honor --mode) before building the plan whenever isolation is in play.
    needs_isolation = bool(decision) and decision.mode in (
        "dual",
        "two-space",
        "remote",
    ) and not backend_space
    forced_mode = None
    mode_arg = getattr(args, "mode", "auto") or "auto"
    if needs_isolation and available:
        if mode_arg in ISOLATION_MODES:
            if mode_arg not in available:
                print(
                    f"error: --mode {mode_arg} is not available for this model "
                    f"(available: {', '.join(available)}).",
                    file=sys.stderr,
                )
                return 2
            forced_mode = mode_arg
        elif mode_arg == "auto" and not args.plan and not args.yes and len(available) > 1:
            default = "dual" if "dual" in available else available[0]
            if decision and decision.mode == "two-space" and "backend" in available:
                default = "backend"
            forced_mode = _prompt_isolation_mode(available, default=default)
        elif mode_arg == "auto" and decision and decision.mode == "two-space":
            # Unattended / --plan: prefer the executable backend Phase 1 over a
            # dead-end two-space message when the repo is pip-installable.
            forced_mode = "backend" if "backend" in available else None

    try:
        plan = decide_plan(
            kind=target.kind,
            slug=target.slug,
            backend_space=backend_space or None,
            decision_mode=decision.mode if decision else None,
            decision_blockers=decision.blockers if decision else None,
            prefer_existing_space=not args.no_discover_space,
            forced_mode=forced_mode,
            pip_installable=pip_installable,
            python_floor=python_floor,
            allow_source_backend=allow_source_backend,
        )
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    predicted_slug = (
        _slug(plan.backend_space)
        if plan.mode == "remote" and plan.backend_space
        else _slug(target.slug)
    )
    predicted_pkg = package_parent / predicted_slug

    steps = build_steps(
        plan,
        target=target,
        target_repo=args.repo or "<owner/space>",
        recipe_path=str(recipe_path),
        package_parent=str(package_parent),
        package_dir=str(predicted_pkg),
        user_token=args.user_token,
        remote_api_name=args.api_name,
        ground_source=target.kind in ("github", "hf_model"),
        inputs=args.inputs,
        outputs=args.outputs,
    )

    _print_plan(target, plan, steps)

    payload = {"ref": target.slug, "kind": target.kind, **plan.to_dict(), "steps": steps}

    if args.plan:
        _emit_json(payload, args.output)
        return 0

    if not plan.can_execute:
        print(
            f"\nThis model needs the '{plan.mode}' architecture, which isn't a single "
            "automated command yet -- follow the guidance above.",
            file=sys.stderr,
        )
        _emit_json(payload, args.output)
        return 2

    if not args.repo:
        print(
            "\nerror: --repo <owner/space> is required to deploy (or pass --plan to "
            "only preview the plan).",
            file=sys.stderr,
        )
        return 2

    if not args.yes:
        try:
            reply = input(f"\nDeploy to {args.repo} using the plan above? [y/N] ").strip().lower()
        except EOFError:
            reply = ""
        if reply not in ("y", "yes"):
            print(
                "Aborted (no changes made). Re-run with --yes to deploy unattended.",
                file=sys.stderr,
            )
            return 0

    recipe_path.parent.mkdir(parents=True, exist_ok=True)
    package_parent.mkdir(parents=True, exist_ok=True)

    # Step 1: generate the recipe (scaffold-remote or generate-recipe-from-llm).
    recipe_step = steps[0]
    print(f"\n$ model-agent {' '.join(recipe_step)}", file=sys.stderr)
    rc = main(recipe_step)
    if rc != 0:
        print(f"  [deploy] recipe step failed (exit {rc}); stopping.", file=sys.stderr)
        return rc

    # The real package dir is <parent>/<slug(model.id)>; recompute it from the
    # recipe we just wrote so the deploy step points at the right folder.
    real_pkg = predicted_pkg
    try:
        recipe = _read_json(recipe_path)
        model_id = str((recipe.get("model") or {}).get("id") or predicted_slug)
        real_pkg = package_parent / _slug(model_id)
    except Exception:
        pass

    gen_step = ["generate-recipe", str(recipe_path), "--output", str(package_parent)]
    if plan.mode == "backend":
        gen_step.append("--backend")
    print(f"\n$ model-agent {' '.join(gen_step)}", file=sys.stderr)
    rc = main(gen_step)
    if rc != 0:
        return rc

    deploy_step = ["deploy-space", str(real_pkg), "--repo", args.repo]
    if plan.mode == "dual":
        deploy_step += ["--sdk", "docker"]
    print(f"\n$ model-agent {' '.join(deploy_step)}", file=sys.stderr)
    rc = main(deploy_step)
    if rc != 0:
        return rc

    print(
        f"\n[deploy] Done -> https://huggingface.co/spaces/{args.repo}",
        file=sys.stderr,
    )
    if plan.mode == "backend":
        print(
            f"  Backend only. When it shows Running, build the HARP frontend with:\n"
            f"    python -m tools.model_agent deploy {target.slug} "
            f"--repo <owner/frontend> --space {args.repo}",
            file=sys.stderr,
        )
    return 0


def _knowledge_query(args) -> dict:
    kb = KnowledgeBase()
    if args.diagnose:
        return {
            "repair_rules": match_repair_rules(args.diagnose),
            "past_failures": kb.find_failures_for_error(args.diagnose),
        }
    if args.similar is not None:
        recipe = _read_json(args.similar)
        fingerprint = fingerprint_for_recipe(recipe) if isinstance(recipe, dict) else ""
        return {
            "deps_fingerprint": fingerprint,
            "matches": kb.find_by_fingerprint(fingerprint),
        }
    if args.find_repo:
        return {"repo": args.find_repo, "matches": kb.find_by_repo(args.find_repo)}
    return {"deployments": kb.deployments()}


def _record_deployment(package_dir: Path, result: dict, *, log=None) -> None:
    """Best-effort: record a successful deploy in the knowledge base."""

    try:
        manifest_path = Path(package_dir) / ".harp" / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
        mode = str(manifest.get("deploy_mode") or "pip")
        record = {
            "repo": str(manifest.get("repo_id") or result.get("repo_id") or ""),
            "model_id": str(manifest.get("repo_id") or ""),
            "mode": {"remote-backend": "remote", "dual-interpreter": "dual"}.get(mode, mode),
            "sdk": str(result.get("sdk") or manifest.get("space_sdk") or "gradio"),
            "space_url": str(result.get("space_url") or ""),
            "backend_space": str(manifest.get("backend_space") or ""),
            "outcome": "success",
        }
        if not record["repo"]:
            return
        KnowledgeBase().record_deployment(record)
        if log is not None:
            log(f"recorded deployment in the knowledge base ({record['repo']} / {record['mode']})")
    except Exception:
        # Recording is a convenience; never fail a real deploy over it.
        pass


def _ensure_github_pip(recipe: dict, requirement: str) -> None:
    """Guarantee a GitHub-grounded recipe installs the repo as a pip dependency.

    The LLM is *asked* to add the ``git+https://...`` requirement to
    ``framework.pip``, but it sometimes omits it -- which silently produces a
    package whose ``requirements.txt`` never installs the upstream code, so the
    wrapper's imports fail at runtime. We therefore inject the canonical
    requirement here unless the LLM already listed the same repo (possibly with
    its own ``@ref``), in which case we keep the LLM's pin.
    """

    if not requirement or not isinstance(recipe, dict):
        return

    framework = recipe.get("framework")
    if not isinstance(framework, dict):
        framework = {}
        recipe["framework"] = framework

    pip = framework.get("pip")
    if not isinstance(pip, list):
        pip = []

    # "git+https://github.com/owner/repo.git" without any trailing "@ref".
    repo_prefix = requirement.rsplit("@", 1)[0]
    already_listed = any(
        isinstance(entry, str) and entry.rsplit("@", 1)[0].strip() == repo_prefix
        for entry in pip
    )
    if not already_listed:
        pip.insert(0, requirement)

    framework["pip"] = pip


def _canon_dist(name: str) -> str:
    """Canonical PyPI project name for comparison (PEP 503-ish)."""

    return re.sub(r"[-_.]+", "-", str(name).strip().split("[")[0]).lower()


def _pip_lists_distribution(pip: list, name: str) -> bool:
    """True when ``pip`` already contains a bare requirement for ``name``."""

    canon = _canon_dist(name)
    for entry in pip:
        if not isinstance(entry, str):
            continue
        text = entry.strip()
        if text.startswith(("git+", "http://", "https://")) or "://" in text:
            continue
        head = re.split(r"[\s<>=!~;@\[]", text, 1)[0]
        if _canon_dist(head) == canon:
            return True
    return False


def _ensure_pip_distribution(recipe: dict, name: str, *, dual: bool) -> None:
    """Ensure ``name`` (a plain PyPI project) is installed, for single or dual.

    Preserves an existing bare requirement for the same project (keeps any LLM
    pin) and never duplicates it. Used to install the repo's published wheel
    instead of a ``git+`` source build.
    """

    if not name or not isinstance(recipe, dict):
        return
    if dual:
        target = _dual_backend_pip(recipe, create=True)
        if target is None:
            return
    else:
        framework = recipe.get("framework")
        if not isinstance(framework, dict):
            framework = {}
            recipe["framework"] = framework
        target = framework.get("pip")
        if not isinstance(target, list):
            target = []
            framework["pip"] = target
    if not _pip_lists_distribution(target, name):
        target.insert(0, name)


def _dual_backend_pip(recipe: dict, *, create: bool = False) -> Optional[list]:
    """Return ``framework.dual.backend_pip`` when present, optionally creating it."""

    if not isinstance(recipe, dict):
        return None
    framework = recipe.get("framework")
    if not isinstance(framework, dict):
        if not create:
            return None
        framework = {}
        recipe["framework"] = framework
    dual = framework.get("dual")
    if not isinstance(dual, dict):
        if not create:
            return None
        dual = {}
        framework["dual"] = dual
    backend_pip = dual.get("backend_pip")
    if not isinstance(backend_pip, list):
        if not create:
            return None
        backend_pip = []
        dual["backend_pip"] = backend_pip
    return backend_pip


def _ensure_github_backend_pip(recipe: dict, requirement: str) -> None:
    """Guarantee a GitHub-grounded dual recipe installs the repo in backend_pip."""

    if not requirement:
        return
    backend_pip = _dual_backend_pip(recipe, create=True)
    if backend_pip is None:
        return
    repo_prefix = requirement.rsplit("@", 1)[0]
    already_listed = any(
        isinstance(entry, str) and entry.rsplit("@", 1)[0].strip() == repo_prefix
        for entry in backend_pip
    )
    if not already_listed:
        backend_pip.insert(0, requirement)


def _strip_repo_pip(recipe: dict, requirement: str) -> None:
    """Remove any ``git+...<repo>`` pip line pointing at the same repo (any @ref)."""

    if not requirement or not isinstance(recipe, dict):
        return
    framework = recipe.get("framework")
    if not isinstance(framework, dict):
        return
    pip = framework.get("pip")
    if not isinstance(pip, list):
        return
    repo_prefix = requirement.rsplit("@", 1)[0]
    framework["pip"] = [
        entry
        for entry in pip
        if not (isinstance(entry, str) and entry.rsplit("@", 1)[0].strip() == repo_prefix)
    ]


def _strip_repo_backend_pip(recipe: dict, requirement: str) -> None:
    """Remove any matching GitHub repo line from ``framework.dual.backend_pip``."""

    if not requirement:
        return
    backend_pip = _dual_backend_pip(recipe)
    if backend_pip is None:
        return
    repo_prefix = requirement.rsplit("@", 1)[0]
    backend_pip[:] = [
        entry
        for entry in backend_pip
        if not (isinstance(entry, str) and entry.rsplit("@", 1)[0].strip() == repo_prefix)
    ]


_PIP_PACKAGING_MARKERS = ("setup.py", "pyproject.toml", "setup.cfg")


def _repo_is_pip_installable(card: object) -> bool:
    """True if a GitHub card's file list has a root Python packaging file.

    ``pip install git+<repo>`` needs ``setup.py`` / ``pyproject.toml`` /
    ``setup.cfg`` at the repo root; without one it fails with "does not appear to
    be a Python project". We only trust a positive signal -- if the file list is
    empty/unknown we assume installable and let pip be the judge.
    """

    if not isinstance(card, dict):
        return True
    files = card.get("files")
    if not isinstance(files, list) or not files:
        return True
    for entry in files:
        path = str(entry).strip().lstrip("./")
        if "/" in path:  # only a ROOT packaging file makes the repo installable
            continue
        if path.lower() in _PIP_PACKAGING_MARKERS:
            return True
    return False


def _pip_installable_from_signals(card: object, manifests: Mapping[str, str]) -> bool:
    """Authoritative pip-installability: trust a fetched root packaging manifest.

    ``fetch_github_dependencies`` GETs ``setup.py`` / ``pyproject.toml`` /
    ``setup.cfg`` directly, so their presence proves the repo is installable even
    when the card's (frequently partial) file listing omits them -- the false
    negative that made omnizart, a PyPI package, look non-installable. Only when no
    packaging manifest was fetched do we fall back to the card's file-list heuristic.
    """

    if manifests and any(name in _PIP_PACKAGING_MARKERS for name in manifests):
        return True
    return _repo_is_pip_installable(card)


def _print_single_space_gate(owner: str, repo: str, decision) -> None:
    """Explain why a --github draft was withheld and point to the right fallback."""

    blockers = "; ".join(decision.blockers) or "unknown"
    print(
        f"  [gate] Not writing a single-Space recipe for {owner}/{repo}: it is not "
        "expected to deploy as one self-contained pyharp Space.\n"
        f"  Blockers: {blockers}\n"
        f"  Recommended mode: {decision.mode}.",
        file=sys.stderr,
    )
    if decision.mode == "remote":
        print(
            "  An existing backend Space can serve it -- proxy to it instead:\n"
            "    python -m tools.model_agent scaffold-remote-recipe <owner/space>\n"
            "    python -m tools.model_agent generate-recipe-from-llm --github "
            f"{owner}/{repo} --remote-space <owner/space> --remote-llm",
            file=sys.stderr,
        )
    elif decision.mode == "dual":
        print(
            "  Use a dual-interpreter Docker recipe (one Space, isolated envs). The "
            "LLM path can now author one with:\n"
            "    python -m tools.model_agent generate-recipe-from-llm --github "
            f"{owner}/{repo} --dual",
            file=sys.stderr,
        )
    else:  # two-space
        print(
            "  Deploy a backend Space, then a thin remote frontend that proxies to it:\n"
            "    python -m tools.model_agent generate-recipe-from-llm --github "
            f"{owner}/{repo} --backend        # build the backend recipe\n"
            "    python -m tools.model_agent scaffold-remote-recipe <your-backend-space>\n"
            "  (If the maintainers publish a Docker image, a backend built FROM it is "
            "less work -- see the omnizart example.)",
            file=sys.stderr,
        )
    print(
        "  Re-run with --emit-anyway to write the draft regardless (it will likely "
        "fail to build).",
        file=sys.stderr,
    )


def _resource_headsup_text(card: object, context) -> str:
    """Concatenate the readable text used to sniff size/GPU cues."""

    chunks: List[str] = []
    if isinstance(card, dict):
        chunks.append(str(card.get("readme") or ""))
    manifests = getattr(context, "dependency_manifests", None) or {}
    sources = getattr(context, "space_sources", None) or {}
    chunks.extend(str(v) for v in manifests.values())
    chunks.extend(str(v) for v in sources.values())
    return "\n".join(chunks)


def _print_resource_headsup(card: object, context) -> None:
    """Print a non-blocking size/GPU heads-up before emitting a local recipe."""

    message = resource_headsup(detect_resource_warnings(_resource_headsup_text(card, context)))
    if message:
        print("  [heads-up] " + message, file=sys.stderr)


def _apply_card_metadata(recipe: dict, card: object) -> None:
    """Fill a scaffolded recipe's model card fields from fetched metadata.

    The remote scaffold leaves ``model.description`` as a TODO and no tags; enrich
    them from the HF/GitHub card so the deployed wrapper shows a real card without
    invoking the LLM.
    """

    if not isinstance(recipe, dict) or not isinstance(card, dict):
        return
    model = recipe.setdefault("model", {})
    if not isinstance(model, dict):
        return
    meta = card.get("meta") if isinstance(card.get("meta"), Mapping) else {}

    tags = [str(tag) for tag in (meta.get("tags") or []) if str(tag).strip()]
    if tags and not model.get("tags"):
        model["tags"] = tags

    license_name = str(meta.get("license") or "").strip()
    if license_name and not model.get("license"):
        model["license"] = license_name

    # Prefer a concise README lead paragraph over the "TODO" placeholder.
    current = str(model.get("description") or "")
    if not current or current.lower().startswith("todo"):
        readme = str(card.get("readme") or "").strip()
        summary = _readme_summary(readme)
        pipeline = str(meta.get("pipeline_tag") or "").strip()
        model["description"] = summary or pipeline or model.get("name") or ""


def _readme_summary(readme: str, *, limit: int = 400) -> str:
    """First real prose paragraph of a README (skips headings/badges/HTML)."""

    for block in readme.split("\n\n"):
        text = block.strip()
        if not text:
            continue
        if text.startswith(("#", "<", "![", "[!", "---", "|")):
            continue
        text = " ".join(text.split())
        if len(text) < 20:
            continue
        return text[:limit]
    return ""


def _emit_recipe_draft(agent: HarpModelAgent, draft, args) -> int:
    """Shared output for the LLM recipe commands: write/print + optional package."""

    _warn_recipe_requirements(draft.recipe)
    if not getattr(args, "no_fix_deps", False):
        _resolve_dependency_conflicts(draft.recipe, agent, fix=True)
    _warn_app_lint(draft.app_py)


    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        write_json(args.output, draft.recipe)

    result = {
        "recipe": draft.recipe,
        "provider": draft.provider,
        "llm_model": draft.model,
        "attempts": draft.attempts,
        "recipe_output": str(args.output) if args.output else None,
    }
    if getattr(args, "generate_package", False) or getattr(args, "smoke_test", False):
        package = build_package_from_recipe(draft.recipe)
        folder = agent.write_generated_app_package(package, args.package_output)
        result["package"] = str(folder)
        if args.smoke_test:
            result["smoke_test"] = _run_smoke_test(
                agent, folder, use_venv=getattr(args, "venv", False)
            ).to_json()
    _emit_json(result, None)
    return 0 if result.get("smoke_test", {}).get("ok", True) else 4


def _run_smoke_test(
    agent: HarpModelAgent,
    package_dir: Path,
    *,
    python_executable: str | None = None,
    startup_timeout_s: float = 180.0,
    use_venv: bool = False,
) -> SmokeTestResult:
    print(
        "WARNING: smoke-test launches the generated app.py, which downloads and "
        "runs third-party model code. Only do this after review or in a sandbox.",
        file=sys.stderr,
    )
    if use_venv and not python_executable:
        print(
            "Preparing isolated venv (first run installs requirements.txt; this can "
            "take several minutes)...",
            file=sys.stderr,
        )
        try:
            python_executable = agent.ensure_package_venv(
                package_dir,
                log=lambda message: print(f"  [venv] {message}", file=sys.stderr),
            )
        except VenvSetupError as exc:
            return SmokeTestResult(ok=False, error=f"venv setup failed: {exc}")
    return agent.smoke_test_package(
        package_dir,
        python_executable=python_executable,
        startup_timeout_s=startup_timeout_s,
    )


def _emit_json(payload: object, output: Path | None) -> None:
    if output:
        output.parent.mkdir(parents=True, exist_ok=True)
        write_json(output, payload)
    print(json.dumps(payload, indent=2, sort_keys=True))


def _read_json(path: Path) -> object:
    return json.loads(path.read_text(encoding="utf-8"))


def _split_csv(value: str) -> List[str]:
    return [item.strip() for item in str(value or "").split(",") if item.strip()]
