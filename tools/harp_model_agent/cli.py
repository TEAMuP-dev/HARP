from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Iterable, List

from .agent import EndpointProbeError, HarpModelAgent, _write_json


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="harp-model-agent",
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
        default=Path("artifacts/harp_model_agent/packages"),
        help="Package output directory.",
    )
    package.add_argument(
        "--no-space-metadata",
        action="store_true",
        help="Skip Hugging Face Space metadata lookup while packaging.",
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

    except EndpointProbeError as exc:
        print(f"Endpoint probe failed: {exc}", file=sys.stderr)
        return 2

    return 1


def _emit_json(payload: object, output: Path | None) -> None:
    if output:
        output.parent.mkdir(parents=True, exist_ok=True)
        _write_json(output, payload)
    print(json.dumps(payload, indent=2, sort_keys=True))


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

