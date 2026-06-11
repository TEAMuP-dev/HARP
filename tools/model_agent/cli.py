from __future__ import annotations

import argparse
import concurrent.futures
import json
import sys
from pathlib import Path
from typing import Iterable, List
from urllib.error import HTTPError, URLError

from .agent import (
    EndpointProbeError,
    HarpModelAgent,
    SmokeTestResult,
    build_generated_app_package,
    render_pyharp_app,
    score_compatibility,
    write_json,
)
from .analyze import analyze_app_file, analyze_path
from .llm import (
    LLMError,
    RecipeGenerationContext,
    complete_recipe,
    default_examples,
    generate_recipe,
    provider_from_env,
)
from .recipe import (
    RecipeError,
    build_package_from_recipe,
    recipe_skeleton_from_analysis,
    render_app_from_recipe,
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

    scaffold_recipe = subparsers.add_parser(
        "scaffold-recipe",
        help="Generate a recipe skeleton from a harvested app.py (fills I/O; stubs inference).",
    )
    scaffold_recipe.add_argument("path", type=Path, help="Path to a harvested app.py file.")
    scaffold_recipe.add_argument("--output", type=Path, help="Optional recipe JSON output path.")

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

    llm_recipe = subparsers.add_parser(
        "generate-recipe-from-llm",
        help="Use an LLM to draft a recipe for any model, then validate + render it.",
    )
    llm_source = llm_recipe.add_mutually_exclusive_group(required=True)
    llm_source.add_argument("--card", type=Path, help="Model-card JSON file (meta/readme/files).")
    llm_source.add_argument("--repo", help="Hugging Face model repo id to fetch the card from (network).")
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
                result["smoke_test"] = _run_smoke_test(agent, folder).to_json()
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

        if args.command == "render-recipe":
            app_py = render_app_from_recipe(_read_json(args.recipe))
            if args.output:
                args.output.parent.mkdir(parents=True, exist_ok=True)
                args.output.write_text(app_py, encoding="utf-8")
            print(app_py)
            return 0

        if args.command == "generate-recipe":
            package = build_package_from_recipe(_read_json(args.recipe))
            folder = agent.write_generated_app_package(package, args.output)
            result = {"package": str(folder), "framework": package.framework}
            if args.smoke_test:
                result["smoke_test"] = _run_smoke_test(agent, folder).to_json()
            _emit_json(result, None)
            return 0 if result.get("smoke_test", {}).get("ok", True) else 4

        if args.command == "generate-recipe-from-llm":
            if args.card:
                card = _read_json(args.card)
            else:
                card = agent.scraper.get_model_card(args.repo)
                if card is None:
                    raise SystemExit(f"Hugging Face model repo not found: {args.repo}")

            context = RecipeGenerationContext.from_card(
                card,
                target_inputs=_split_csv(args.inputs),
                target_outputs=_split_csv(args.outputs),
                examples=[] if args.no_examples else default_examples(),
            )
            provider = provider_from_env(
                args.provider,
                model=args.llm_model,
                timeout=args.llm_timeout,
                temperature=args.temperature,
            )
            draft = generate_recipe(context, provider, max_repairs=args.max_repairs)
            return _emit_recipe_draft(agent, draft, args)

        if args.command == "complete-recipe":
            base = _read_json(args.recipe)
            context = None
            if args.card:
                context = RecipeGenerationContext.from_card(_read_json(args.card))
            elif args.repo:
                card = agent.scraper.get_model_card(args.repo)
                if card is None:
                    raise SystemExit(f"Hugging Face model repo not found: {args.repo}")
                context = RecipeGenerationContext.from_card(card)
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
                result["smoke_test"] = _run_smoke_test(agent, folder).to_json()
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
            )
            _emit_json(result.to_json(), None)
            return 0 if result.ok else 4

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


def _emit_recipe_draft(agent: HarpModelAgent, draft, args) -> int:
    """Shared output for the LLM recipe commands: write/print + optional package."""

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
            result["smoke_test"] = _run_smoke_test(agent, folder).to_json()
    _emit_json(result, None)
    return 0 if result.get("smoke_test", {}).get("ok", True) else 4


def _run_smoke_test(
    agent: HarpModelAgent,
    package_dir: Path,
    *,
    python_executable: str | None = None,
    startup_timeout_s: float = 180.0,
) -> SmokeTestResult:
    print(
        "WARNING: smoke-test launches the generated app.py, which downloads and "
        "runs third-party model code. Only do this after review or in a sandbox.",
        file=sys.stderr,
    )
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
