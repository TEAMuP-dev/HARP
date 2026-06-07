from __future__ import annotations

import argparse
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
from .analyze import analyze_path


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


def _emit_json(payload: object, output: Path | None) -> None:
    if output:
        output.parent.mkdir(parents=True, exist_ok=True)
        write_json(output, payload)
    print(json.dumps(payload, indent=2, sort_keys=True))


def _read_json(path: Path) -> object:
    return json.loads(path.read_text(encoding="utf-8"))


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
