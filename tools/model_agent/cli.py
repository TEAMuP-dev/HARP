from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Iterable, List

from .agent import (
    EndpointProbeError,
    HarpModelAgent,
    build_generated_app_package,
    render_pyharp_app,
    score_compatibility,
    _write_json,
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
            _emit_json({"package": str(folder)}, None)
            return 0

        if args.command == "package-repo":
            package = agent.build_generated_app_package_for_repo(args.repo)
            folder = agent.write_generated_app_package(package, args.output)
            _emit_json({"package": str(folder), "repo_id": package.repo_id}, None)
            return 0

    except EndpointProbeError as exc:
        print(f"Endpoint probe failed: {exc}", file=sys.stderr)
        return 2
    except ValueError as exc:
        print(f"Packaging failed: {exc}", file=sys.stderr)
        return 2
    except NotImplementedError as exc:
        print(f"Template generation failed: {exc}", file=sys.stderr)
        return 3

    return 1


def _emit_json(payload: object, output: Path | None) -> None:
    if output:
        output.parent.mkdir(parents=True, exist_ok=True)
        _write_json(output, payload)
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
