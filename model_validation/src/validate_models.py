#!/usr/bin/env python3
"""
HARP model validation - command-line entry point.

Validates HARP deployments in two tiers, using the same black-box harness
(see harness.py):

1. Spaces (default): discovers all Spaces under an organization (default:
   teamup-tech), verifies each is running (restarting crashed/stopped spaces
   by default), exposes the HARP gradio endpoints (/controls and /process),
   and processes test inputs end-to-end. ZeroGPU spaces require an
   authenticated request for GPU quota, so a token must be provided via the
   HF_TOKEN environment variable - never on the command line or in the
   repository.

2. Examples (--local-examples): launches each app under pyharp/examples/ on
   a local port and runs the identical endpoint tests. The examples tier
   exercises pyharp itself with no Hugging Face infrastructure in the loop,
   so a failure indicates a pyharp/gradio-level breakage rather than a
   deployment-specific one.

Per-model test cases are declared in config.yml (see README.md for a full
walkthrough). When a model has no configured cases, a single "default" case
runs with inputs synthesized automatically from the /controls spec.

Usage:
    HF_TOKEN=... python model_validation/src/validate_models.py
    HF_TOKEN=... python model_validation/src/validate_models.py --spaces teamup-tech/pitch_shifter
    HF_TOKEN=... python model_validation/src/validate_models.py --load-only --workers 8
    HF_TOKEN=... python model_validation/src/validate_models.py --no-restart-failed
    python model_validation/src/validate_models.py --local-examples

Exit codes:
    0 - all validated models passed (or were explicitly skipped)
    1 - at least one model failed
    2 - infrastructure/configuration error (bad token, no spaces found, ...)
"""

import argparse
import concurrent.futures
import os
import sys
import threading
import time
from pathlib import Path

from assets import Assets
from harness import test_space, test_local_example
from quota import ZeroGPUTracker, is_zerogpu
from results import PASS, FAIL, SKIP, status_emoji, write_reports
from utils import get_token, scrub, load_config, get_excluded, discover_spaces


DEFAULT_ORG = "teamup-tech"
SCRIPT_DIR = Path(__file__).parent            # model_validation/src
MODEL_VALIDATION_DIR = SCRIPT_DIR.parent      # model_validation
REPO_ROOT = MODEL_VALIDATION_DIR.parent
DEFAULT_CONFIG = MODEL_VALIDATION_DIR / "config.yml"
DEFAULT_EXAMPLES_DIR = REPO_ROOT / "pyharp" / "examples"
DEFAULT_OUTPUT_DIR = MODEL_VALIDATION_DIR / "reports"

LOCAL_PORT_BASE = 7861


def parse_args() -> argparse.Namespace:
    """
    Define and parse the command-line interface.

    Returns:
        opts (argparse.Namespace): Parsed options.
    """

    parser = argparse.ArgumentParser(description="Validate HARP model deployments.")
    parser.add_argument("--org", default=DEFAULT_ORG, help="HF organization to scan")
    parser.add_argument("--spaces", nargs="*", default=None,
                        help="Explicit space ids to validate (skips discovery)")
    parser.add_argument("--exclude", nargs="*", default=None, metavar="MODEL",
                        help="Models to exclude from validation (space ids, or "
                             "examples/<example-dir>); merged with the config "
                             "`exclude` list")
    parser.add_argument("--local-examples", nargs="*", default=None, metavar="DIR",
                        help="Validate local pyharp example apps instead of remote "
                             "spaces (the examples tier). With no DIRs given, tests "
                             "every app under pyharp/examples/.")
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG,
                        help="Optional YAML config (excludes, per-model overrides/cases)")
    parser.add_argument("--load-only", action="store_true",
                        help="Only verify availability and /controls, do not run inference")
    parser.add_argument("--skip-zerogpu", action="store_true",
                        help="Skip models on ZeroGPU hardware, to avoid spending "
                             "the shared ZeroGPU allowance")
    parser.add_argument("--restart-failed", action=argparse.BooleanOptionalAction,
                        default=True,
                        help="Attempt to restart spaces found in an error/stopped state; "
                             "enabled by default (requires a token with write access), "
                             "disable with --no-restart-failed")
    parser.add_argument("--workers", type=int, default=4,
                        help="Number of spaces to validate concurrently (spaces tier only)")
    parser.add_argument("--zerogpu-workers", type=int, default=1,
                        help="How many ZeroGPU models may make GPU calls at once. "
                             "Each concurrent call reserves its declared duration, "
                             "so 1 (the default) keeps overlapping reservations "
                             "from tying up the allowance")
    parser.add_argument("--connect-timeout", type=float, default=420,
                        help="Seconds to wait for a deployment to build/wake/start")
    parser.add_argument("--process-timeout", type=float, default=600,
                        help="Seconds to wait for /process on non-ZeroGPU models")
    parser.add_argument("--zerogpu-process-timeout", type=float, default=120,
                        help="Seconds allowed for /process EXECUTION on ZeroGPU "
                             "models once they leave the queue (queue wait is "
                             "bounded separately by --connect-timeout; default 120)")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR,
                        help=f"Directory for reports, synthesized assets, and "
                             f"example logs (default: {DEFAULT_OUTPUT_DIR})")
    parser.add_argument("--verbose", action="store_true")

    return parser.parse_args()


def result_line(result, extra: str = "", token: str = "") -> str:
    """
    Format one model's console line.

    Args:
        result (ModelResult): The completed validation record.
        extra (str): Text inserted before the error note (e.g. hardware).
        token (str): Token to scrub from the error text.

    Returns:
        line (str): The formatted line.
    """

    note = f" - {result.error}" if result.error else ""

    return (f"{status_emoji(result)} {result.status:4s} {result.target} "
            f"({result.duration}s){extra}{scrub(note, token)}")


def validate_examples(opts: argparse.Namespace, config: dict, excluded: set,
                      assets: Assets) -> list:
    """
    Run the examples tier: launch and validate each local pyharp example.

    Examples run sequentially so concurrent model loads cannot exhaust the
    machine's memory.

    Args:
        opts (argparse.Namespace): Parsed command-line options.
        config (dict): Parsed configuration.
        excluded (set): Model keys to leave out.
        assets (Assets): Synthesized input files.

    Returns:
        results (list): ModelResult objects, or None when no examples exist.
    """

    overrides = config.get("overrides", {})

    if opts.local_examples:
        app_dirs = [Path(d) for d in opts.local_examples]
    else:
        app_dirs = sorted(d for d in DEFAULT_EXAMPLES_DIR.iterdir()
                          if (d / "app.py").exists())
    app_dirs = [d for d in app_dirs if f"examples/{d.name}" not in excluded]

    if not app_dirs:
        print("ERROR: no local examples found", file=sys.stderr)
        return None

    print(f"Validating {len(app_dirs)} pyharp examples\n")

    results = []
    for i, app_dir in enumerate(app_dirs):
        r = test_local_example(app_dir, LOCAL_PORT_BASE + i, assets, opts,
                               overrides.get(f"examples/{app_dir.name}", {}))
        results.append(r)
        print(result_line(r))

    return results


def validate_spaces(opts: argparse.Namespace, config: dict, excluded: set,
                    assets: Assets, token: str) -> list:
    """
    Run the spaces tier: validate remote Hugging Face Spaces concurrently.

    Args:
        opts (argparse.Namespace): Parsed command-line options.
        config (dict): Parsed configuration.
        excluded (set): Model keys to leave out.
        assets (Assets): Synthesized input files.
        token (str): Hugging Face access token.

    Returns:
        results (list): ModelResult objects, or None when no spaces exist.
    """

    from huggingface_hub import HfApi

    overrides = config.get("overrides", {})
    api = HfApi(token=token)

    if opts.spaces:
        space_ids = [s for s in opts.spaces if s not in excluded]
    else:
        space_ids = discover_spaces(api, opts.org, config, excluded)

    if not space_ids:
        print(f"ERROR: no spaces found for org '{opts.org}'", file=sys.stderr)
        return None

    tracker = ZeroGPUTracker(config.get("zerogpu_budget_seconds"))
    quota_exhausted = threading.Event()
    zerogpu_limiter = threading.Semaphore(max(1, opts.zerogpu_workers))
    print(f"Validating {len(space_ids)} spaces with {opts.workers} workers "
          f"({opts.zerogpu_workers} concurrent on ZeroGPU; "
          f"process test: {'OFF' if opts.load_only else 'ON'})\n")

    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=opts.workers) as pool:
        futures = [pool.submit(test_space, sid, token, assets, opts,
                               overrides.get(sid, {}), quota_exhausted, zerogpu_limiter)
                   for sid in space_ids]
        for future in concurrent.futures.as_completed(futures):
            r = future.result()
            results.append(r)
            # Show the hardware for every model; ZeroGPU work only accrues for
            # ZeroGPU models. Count each /process call that reached the GPU and
            # its total wall time - a queued or input-skipped case never ran
            # and is excluded
            info = r.hardware or "?"
            if is_zerogpu(r.hardware):
                ran = [c for c in r.cases if c.executed]
                tracker.add(len(ran), sum(c.duration for c in ran))
                info = f"{info} | {tracker.summary()}"
            print(result_line(r, f" [{info}]", token))

    return results


def main() -> int:
    """
    Entry point: run the selected tier and write reports.

    Returns:
        code (int): Process exit code (see module docstring).
    """

    opts = parse_args()
    config = load_config(opts.config)
    excluded = get_excluded(config, opts.exclude)
    # Generic cases applied to every model, on top of its own (see config.yml)
    opts.common_test_cases = config.get("common_test_cases", [])

    # Each run writes to its own timestamped directory so runs never overwrite
    # each other; assets, logs, and reports all live under it. Local time is
    # used for the directory name (on a UTC CI runner this is naturally UTC).
    run_stamp = time.strftime("%Y-%m-%dT%H-%M-%S", time.localtime())
    opts.output_dir = opts.output_dir / run_stamp
    assets = Assets(opts.output_dir / "assets",
                    config.get("synthesized_inputs"))

    if opts.local_examples is not None:
        token = ""
        label = "pyharp examples"
        results = validate_examples(opts, config, excluded, assets)
    else:
        token = get_token(required=True)
        label = f"{opts.org} spaces"
        results = validate_spaces(opts, config, excluded, assets, token)

    if results is None:
        return 2

    # Record how the run was invoked (argv holds no secrets - the token comes
    # from HF_TOKEN) plus the resolved options, so the report is reproducible
    command = " ".join(sys.argv)
    options = {k: str(v) if isinstance(v, Path) else v
               for k, v in vars(opts).items()}
    write_reports(results, opts.output_dir, label, command, options)

    passed = [r for r in results if r.status == PASS]
    failed = [r for r in results if r.status == FAIL]
    skipped = [r for r in results if r.status == SKIP]
    validated = len(results) - len(skipped)   # exclude skipped from the total
    summary = f"\n{len(passed)}/{validated} models passed"
    if skipped:
        summary += f", {len(skipped)} skipped"
    print(f"{summary}. Reports written to {opts.output_dir}/")
    if failed:
        print("\nFailed models:")
        for r in sorted(failed, key=lambda r: r.target):
            print(f"  - {r.target}: {scrub(r.error, token)}")

    return 1 if failed else 0


if __name__ == "__main__":
    code = main()
    # Hard-exit rather than sys.exit: gradio_client and timed-out calls can
    # leave non-daemon threads behind, and a normal interpreter shutdown
    # would join them - hanging the process after all results are printed.
    # Reports are already flushed to disk at this point.
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(code)
