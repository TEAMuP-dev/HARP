from __future__ import annotations

import argparse
import concurrent.futures
import json
import sys
from pathlib import Path
from typing import Iterable, List, Mapping
from urllib.error import HTTPError, URLError

from .agent import (
    HARP_GRADIO_VERSION,
    DeploySpaceError,
    EndpointProbeError,
    lint_generated_app,
    HarpModelAgent,
    SmokeTestResult,
    VenvSetupError,
    build_generated_app_package,
    render_pyharp_app,
    score_compatibility,
    write_json,
)
from .analyze import analyze_app_file, analyze_path
from .llm import (
    LLMError,
    RecipeDraft,
    RecipeGenerationContext,
    complete_recipe,
    default_examples,
    generate_recipe,
    provider_from_env,
    refine_remote_recipe,
)
from .recipe import (
    RecipeError,
    build_package_from_recipe,
    lint_recipe_requirements,
    recipe_skeleton_from_analysis,
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

    probe = subparsers.add_parser("probe", help="Fetch HARP controls from model endpoints.")
    probe.add_argument("models", nargs="+", help="HARP model paths, HF Space paths, or endpoint URLs.")
    probe.add_argument("--output", type=Path, help="Optional JSON output path.")

    package = subparsers.add_parser("package", help="Package HARP controls into review folders.")
    package.add_argument("models", nargs="*", help="Model paths to package.")
    package.add_argument("--from-file", type=Path, help="Read model ids from a discovery/probe JSON file.")
    package.add_argument(
        "--output",
        type=Path,
        default=Path("artifacts/model_agent/packages"),
        help="Package output directory.",
    )
    package.add_argument(
        "--no-space-metadata",
        action="store_true",
        help="Skip Hugging Face Space metadata lookup while packaging.",
    )

    score = subparsers.add_parser("score-card", help="Score a raw Hugging Face model card JSON file.")
    score.add_argument("card", type=Path, help="JSON file with meta/readme/files fields.")
    score.add_argument("--output", type=Path, help="Optional JSON output path.")

    render = subparsers.add_parser("render-app", help="Render a starter pyharp app.py from a model card.")
    render.add_argument("card", type=Path, help="JSON file with meta/readme/files fields.")
    render.add_argument("--output", type=Path, help="Optional app.py output path.")

    generate = subparsers.add_parser(
        "generate-package",
        help="Write app.py, requirements.txt, and manifest.json for a raw model card.",
    )
    generate.add_argument("card", type=Path, help="JSON file with meta/readme/files fields.")
    generate.add_argument(
        "--output",
        type=Path,
        default=Path("artifacts/model_agent/generated"),
        help="Generated package output directory.",
    )
    generate.add_argument(
        "--smoke-test",
        action="store_true",
        help="After writing, launch app.py and verify HARP controls (runs downloaded code).",
    )
    _add_venv_flag(generate)

    scaffold_recipe = subparsers.add_parser(
        "scaffold-recipe",
        help="Generate a recipe skeleton from a harvested app.py (fills I/O; stubs inference).",
    )
    scaffold_recipe.add_argument("path", type=Path, help="Path to a harvested app.py file.")
    scaffold_recipe.add_argument("--output", type=Path, help="Optional recipe JSON output path.")

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
        "--inputs", default="", help="Comma-separated desired input types (e.g. audio,slider)."
    )
    llm_recipe.add_argument(
        "--outputs", default="", help="Comma-separated desired output types (e.g. audio,labels)."
    )
    llm_recipe.add_argument(
        "--provider",
        choices=["gemini", "anthropic", "openai"],
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
    _add_venv_flag(llm_recipe)

    list_models = subparsers.add_parser(
        "list-models",
        help="List LLM models that support content generation for the configured provider.",
    )
    list_models.add_argument(
        "--provider",
        choices=["gemini", "anthropic", "openai"],
        help="LLM provider (default: auto-detect from API key env vars).",
    )
    list_models.add_argument("--llm-timeout", type=float, default=60.0, help="API timeout (s).")

    complete = subparsers.add_parser(
        "complete-recipe",
        help="Use an LLM to fill the _todo stubs of a scaffolded recipe (preserves I/O).",
    )
    complete.add_argument("recipe", type=Path, help="Scaffolded recipe JSON (from scaffold-recipe).")
    complete.add_argument(
        "--card", type=Path, help="Optional model-card JSON to enrich the prompt with the README."
    )
    complete.add_argument(
        "--repo", help="Optional Hugging Face repo id to fetch the card from (network)."
    )
    complete.add_argument(
        "--provider",
        choices=["gemini", "anthropic", "openai"],
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

    package_repo = subparsers.add_parser(
        "package-repo",
        help="Fetch a Hugging Face model repo and write a HARP Space package.",
    )
    package_repo.add_argument("repo", help="Hugging Face model repo id or URL.")
    package_repo.add_argument(
        "--output",
        type=Path,
        default=Path("artifacts/model_agent/hf_spaces"),
        help="Generated Hugging Face Space repo output directory.",
    )
    package_repo.add_argument(
        "--smoke-test",
        action="store_true",
        help="After writing, launch app.py and verify HARP controls (runs downloaded code).",
    )
    _add_venv_flag(package_repo)

    harvest = subparsers.add_parser(
        "harvest",
        help="Download app.py from an author's HF Spaces for offline review.",
    )
    harvest.add_argument(
        "--author",
        default="teamup-tech",
        help="Hugging Face author/org whose Spaces to harvest.",
    )
    harvest.add_argument("--query", default="", help="Optional search query.")
    harvest.add_argument("--limit", type=int, default=100, help="Maximum Spaces to fetch.")
    harvest.add_argument(
        "--filename",
        default="app.py",
        help="File to download from each Space.",
    )
    harvest.add_argument(
        "--output",
        type=Path,
        default=Path("artifacts/model_agent/harvest"),
        help="Directory to write harvested files into.",
    )

    analyze = subparsers.add_parser(
        "analyze",
        help="Statically analyze harvested app.py files and report I/O shapes.",
    )
    analyze.add_argument(
        "path",
        type=Path,
        help="A harvested directory (searched recursively) or a single app.py file.",
    )
    analyze.add_argument(
        "--filename",
        default="app.py",
        help="File name to look for when PATH is a directory.",
    )
    analyze.add_argument("--output", type=Path, help="Optional JSON output path.")
    analyze.add_argument(
        "--summary-only",
        action="store_true",
        help="Emit only the aggregate summary, not the per-app records.",
    )
    analyze.add_argument(
        "--check-health",
        action="store_true",
        help="Also probe each harvested Space's endpoint (uses index.json; network).",
    )
    analyze.add_argument(
        "--health-timeout",
        type=float,
        default=20.0,
        help="Per-Space timeout in seconds for --check-health probes.",
    )
    analyze.add_argument(
        "--health-workers",
        type=int,
        default=8,
        help="Number of concurrent --check-health probes.",
    )

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

        if args.command == "probe":
            payload = []
            for model in args.models:
                controls = agent.endpoint_client.fetch_controls(model)
                payload.append({"model_path": model, "controls": controls})
            _emit_json(payload, args.output)
            return 0

        if args.command == "package":
            models = list(args.models)
            if args.from_file:
                models.extend(_models_from_file(args.from_file))
            if not models:
                raise SystemExit("package requires at least one model or --from-file")

            written = []
            for model in models:
                package = agent.package_model(
                    model,
                    include_space_metadata=not args.no_space_metadata,
                )
                written.append(str(agent.write_package(package, args.output)))
            _emit_json({"packages": written}, None)
            return 0

        if args.command == "score-card":
            result = score_compatibility(_read_json(args.card))
            _emit_json(result, args.output)
            return 0

        if args.command == "render-app":
            app_py = render_pyharp_app(_read_json(args.card))
            if args.output:
                args.output.parent.mkdir(parents=True, exist_ok=True)
                args.output.write_text(app_py, encoding="utf-8")
            print(app_py)
            return 0

        if args.command == "generate-package":
            package = build_generated_app_package(_read_json(args.card))
            folder = agent.write_generated_app_package(package, args.output)
            result = {"package": str(folder), "framework": package.framework}
            if args.smoke_test:
                result["smoke_test"] = _run_smoke_test(
                    agent, folder, use_venv=getattr(args, "venv", False)
                ).to_json()
            _emit_json(result, None)
            return 0 if result.get("smoke_test", {}).get("ok", True) else 4

        if args.command == "scaffold-recipe":
            record = analyze_app_file(args.path)
            if not record.get("recipe_eligible"):
                reason = record.get("unresolved_reason", "components could not be resolved")
                print(
                    f"Cannot scaffold a recipe from {args.path}: {reason}.",
                    file=sys.stderr,
                )
                return 2
            model_id = _harvest_ids_by_slug(args.path).get(Path(args.path).parent.name, "")
            recipe = recipe_skeleton_from_analysis(record, model_id=model_id)
            _emit_json(recipe, args.output)
            return 0

        if args.command == "scaffold-remote-recipe":
            print(
                f"Probing the backend Space's Gradio API: {args.space} ...",
                file=sys.stderr,
            )
            recipe = agent.scaffold_remote_recipe(args.space, api_name=args.api_name or None)
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
                scaffold = remote_recipe_from_api_info(
                    canonical, api_info, api_name=args.remote_api_name or None
                )
                if args.remote_llm:
                    chosen = str(scaffold["framework"]["remote"]["api_name"])
                    endpoint = (api_info.get("named_endpoints") or {}).get(chosen, {})
                    print(
                        f"  Refining the scaffold with the LLM (grounded on '{chosen}'; "
                        "space/api_name and arg order stay pinned) ...",
                        file=sys.stderr,
                    )
                    provider = provider_from_env(
                        args.provider,
                        model=args.llm_model,
                        timeout=args.llm_timeout,
                        temperature=args.temperature,
                    )
                    llm_context = RecipeGenerationContext.from_card(card)
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
                examples=[] if args.no_examples else default_examples(),
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
            )
            if github_target is not None:
                if _repo_is_pip_installable(card):
                    _ensure_github_pip(draft.recipe, context.source_repo_url)
                else:
                    # No setup.py/pyproject.toml -> `pip install git+<repo>` fails with
                    # "does not appear to be a Python project". Strip that doomed line
                    # (the LLM often adds it) and steer to remote-backend mode instead.
                    _strip_repo_pip(draft.recipe, context.source_repo_url)
                    owner, repo, _ref = github_target
                    print(
                        f"  [deps] WARNING: {owner}/{repo} has no setup.py/pyproject.toml, "
                        "so it is NOT pip-installable -- a non-remote wrapper that imports "
                        "it cannot work (this is the 'does not appear to be a Python "
                        "project' build error). Deploy it as a remote-backend proxy to its "
                        "Gradio Space instead, e.g.:\n"
                        f"    python -m tools.model_agent scaffold-remote-recipe <its-hf-space>\n"
                        f"    python -m tools.model_agent generate-recipe-from-llm --github "
                        f"{owner}/{repo} --remote-space <its-hf-space>",
                        file=sys.stderr,
                    )
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

        if args.command == "package-repo":
            package = agent.build_generated_app_package_for_repo(args.repo)
            folder = agent.write_generated_app_package(package, args.output)
            result = {
                "package": str(folder),
                "repo_id": package.repo_id,
                "framework": package.framework,
            }
            if args.smoke_test:
                result["smoke_test"] = _run_smoke_test(
                    agent, folder, use_venv=getattr(args, "venv", False)
                ).to_json()
            _emit_json(result, None)
            return 0 if result.get("smoke_test", {}).get("ok", True) else 4

        if args.command == "harvest":
            results = agent.harvest_space_apps(
                args.output,
                author=args.author,
                query=args.query,
                limit=args.limit,
                filename=args.filename,
            )
            summary = {"ok": 0, "missing": 0, "error": 0}
            for record in results:
                summary[record["status"]] = summary.get(record["status"], 0) + 1
            _emit_json(
                {"output": str(args.output), "summary": summary, "results": results},
                None,
            )
            return 0

        if args.command == "analyze":
            report = analyze_path(args.path, filename=args.filename)
            if args.check_health:
                attach_health(
                    agent,
                    args.path,
                    report,
                    timeout=args.health_timeout,
                    max_workers=args.health_workers,
                )
            if args.summary_only:
                report = report["summary"]
            _emit_json(report, args.output)
            return 0

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
            _emit_json(result, None)
            return 0

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
    except NotImplementedError as exc:
        print(f"Template generation failed: {exc}", file=sys.stderr)
        return 3
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


def _harvest_ids_by_slug(path: Path) -> dict:
    """Map a harvest folder slug -> canonical Space id, using index.json.

    Locates ``index.json`` at ``path`` (when it is the harvest dir) or among its
    ancestors (when ``path`` points at a single harvested ``app.py``).
    """

    path = Path(path)
    candidates = []
    if path.is_dir():
        candidates.append(path / "index.json")
    candidates.extend(parent / "index.json" for parent in path.parents)

    index_file = next((candidate for candidate in candidates if candidate.exists()), None)
    if index_file is None:
        return {}

    data = json.loads(index_file.read_text(encoding="utf-8"))
    entries = data.get("results", data) if isinstance(data, dict) else data

    mapping: dict = {}
    if isinstance(entries, list):
        for entry in entries:
            if not isinstance(entry, dict):
                continue
            entry_path = entry.get("path") or ""
            model_id = entry.get("id")
            if entry_path and model_id:
                mapping[Path(entry_path).parent.name] = model_id
    return mapping


def attach_health(
    agent: HarpModelAgent,
    path: Path,
    report: dict,
    *,
    timeout: float = 20.0,
    max_workers: int = 8,
) -> None:
    """Add a liveness probe to each analyzed app, joined via harvest index.json.

    Probes run concurrently with a short per-Space timeout so a handful of
    sleeping/dead Spaces don't serialize into a multi-minute wait.
    """

    ids_by_slug = _harvest_ids_by_slug(path)

    # Use a short timeout for liveness so dead Spaces fail fast rather than
    # blocking for the full discovery/probe timeout.
    agent.endpoint_client.timeout = timeout

    jobs = []
    for record in report.get("apps", []):
        record_path = record.get("path") or ""
        slug = Path(record_path).parent.name if record_path else ""
        model_id = ids_by_slug.get(slug)
        if model_id:
            jobs.append((record, model_id))
        else:
            record["health"] = {"status": "unknown", "reason": "no Space id in index.json"}

    total = len(jobs)
    print(
        f"Probing {total} Space endpoint(s) with timeout {timeout:.0f}s "
        f"across {max_workers} worker(s)...",
        file=sys.stderr,
    )

    def probe(job):
        record, model_id = job
        health = agent.check_endpoint_health(model_id)
        health["model_id"] = model_id
        return record, health

    done = 0
    if jobs:
        with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, max_workers)) as executor:
            futures = [executor.submit(probe, job) for job in jobs]
            for future in concurrent.futures.as_completed(futures):
                record, health = future.result()
                record["health"] = health
                done += 1
                print(
                    f"  [{done}/{total}] {health['model_id']}: {health['status']}",
                    file=sys.stderr,
                )

    alive = dead = unknown = 0
    for record in report.get("apps", []):
        status = record.get("health", {}).get("status")
        if status == "alive":
            alive += 1
        elif status == "dead":
            dead += 1
        else:
            unknown += 1

    report.setdefault("summary", {})["health"] = {
        "alive": alive,
        "dead": dead,
        "unknown": unknown,
    }


def _emit_json(payload: object, output: Path | None) -> None:
    if output:
        output.parent.mkdir(parents=True, exist_ok=True)
        write_json(output, payload)
    print(json.dumps(payload, indent=2, sort_keys=True))


def _read_json(path: Path) -> object:
    return json.loads(path.read_text(encoding="utf-8"))


def _split_csv(value: str) -> List[str]:
    return [item.strip() for item in str(value or "").split(",") if item.strip()]


def _models_from_file(path: Path) -> List[str]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    models: List[str] = []
    records = payload if isinstance(payload, list) else payload.get("packages", [])
    for item in records:
        if isinstance(item, str):
            models.append(item)
        elif isinstance(item, dict):
            model = item.get("model_path") or item.get("id")
            if model:
                models.append(str(model))
    return models
