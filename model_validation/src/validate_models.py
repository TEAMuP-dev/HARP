#!/usr/bin/env python3
"""
HARP model validation - command-line entry point.

Parses the options, runs one of the two tiers, and writes the reports:

1. Spaces (default): every Space under an organization, validated concurrently
   by spaces.py. Requires a Hugging Face token, read only from the HF_TOKEN
   environment variable so it never appears in argv or in the repository.

2. Examples (--local-examples): the pyharp example apps, launched locally and
   validated one at a time by examples.py. Needs no token, and exercises
   pyharp itself with no Hugging Face infrastructure in the loop.

Either way the checks are the same, and come from harness.py. Which models to
validate and what to assert about their outputs is configured in config.yml.
README.md is the guide to writing that.

Usage:
    HF_TOKEN=... python model_validation/src/validate_models.py
    HF_TOKEN=... python model_validation/src/validate_models.py --spaces pitch_shifter
    HF_TOKEN=... python model_validation/src/validate_models.py --skip-zerogpu
    python model_validation/src/validate_models.py --local-examples pitch_shifter

Exit codes:
    0 - every validated model passed (skipped models do not count against it)
    1 - at least one model failed
    2 - the run could not proceed (missing token, no models found, bad config)
"""

import argparse
import concurrent.futures
import os
import sys
import threading
import time
from pathlib import Path

from assets import Assets, check_synthesized_inputs
from examples import test_local_example
from spaces import test_space
from quota import ZeroGPUTracker, is_zerogpu
from results import FAIL, status_emoji, write_reports
from utils import (get_token, scrub, load_config, check_config_keys, qualify,
                   qualify_keys, Exclusions, get_excluded, discover_spaces)


DEFAULT_ORG = "teamup-tech"
SCRIPT_DIR = Path(__file__).parent            # model_validation/src
MODEL_VALIDATION_DIR = SCRIPT_DIR.parent      # model_validation
REPO_ROOT = MODEL_VALIDATION_DIR.parent
DEFAULT_CONFIG = MODEL_VALIDATION_DIR / "config.yml"
DEFAULT_EXAMPLES_DIR = REPO_ROOT / "pyharp" / "examples"
DEFAULT_OUTPUT_DIR = MODEL_VALIDATION_DIR / "reports"

LOCAL_PORT_BASE = 7861

# Cap on the error text shown on a model's console line (see result_line)
CONSOLE_ERROR_CHARS = 200


def parse_args() -> argparse.Namespace:
    """
    Define and parse the command-line interface.

    Returns:
        opts (argparse.Namespace): Parsed options.
    """

    parser = argparse.ArgumentParser(description="Validate HARP model deployments.")
    parser.add_argument("--org", default=DEFAULT_ORG,
                        help=f"HF organization whose spaces are discovered, and "
                             f"the owner a bare space name is taken to have "
                             f"(default: {DEFAULT_ORG})")
    parser.add_argument("--spaces", nargs="*", default=None,
                        help="Explicit spaces to validate, skipping discovery. A "
                             "bare name is taken to be in --org. Give 'owner/name' "
                             "to reach another organization")
    parser.add_argument("--exclude", nargs="*", default=None, metavar="MODEL",
                        help="Models to exclude, in either tier. A bare name means "
                             "the model in the tier being run. Qualify it "
                             "('owner/name' or 'examples/<dir>') to pin it to one. "
                             "Merged with the config `exclude` list")
    parser.add_argument("--local-examples", nargs="*", default=None, metavar="DIR",
                        help="Validate local pyharp example apps instead of remote "
                             "spaces. Name them by directory ('pitch_shifter' or "
                             "'examples/pitch_shifter'), or give a path to an example "
                             "kept elsewhere. With none given, every app under "
                             "pyharp/examples/ is validated")
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG,
                        help="Optional YAML config (excludes, per-model overrides/cases)")
    parser.add_argument("--load-only", action="store_true",
                        help="Only verify availability and /controls, do not run inference")
    parser.add_argument("--skip-zerogpu", action="store_true",
                        help="Skip models on ZeroGPU hardware, to avoid spending "
                             "the shared ZeroGPU allowance")
    parser.add_argument("--restart-failed", action=argparse.BooleanOptionalAction,
                        default=True,
                        help="Attempt to restart spaces found in an error or stopped "
                             "state. Enabled by default, and requires a token with "
                             "write access. Disable with --no-restart-failed")
    parser.add_argument("--workers", type=int, default=4,
                        help="Number of spaces to validate concurrently (spaces tier only)")
    parser.add_argument("--zerogpu-workers", type=int, default=1,
                        help="How many ZeroGPU models may make GPU calls at once. "
                             "Each concurrent call reserves its declared duration, "
                             "so 1 (the default) keeps overlapping reservations "
                             "from tying up the allowance")
    parser.add_argument("--connect-timeout", type=float, default=420,
                        help="Seconds to wait for a deployment to build, wake, "
                             "or start (default: 420)")
    parser.add_argument("--process-timeout", type=float, default=600,
                        help="Seconds to wait for /process on non-ZeroGPU "
                             "models (default: 600)")
    parser.add_argument("--zerogpu-process-timeout", type=float, default=120,
                        help="Seconds allowed for /process EXECUTION on ZeroGPU "
                             "models once they leave the queue, with the queue "
                             "wait bounded separately by --connect-timeout "
                             "(default: 120)")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR,
                        help=f"Directory for reports, synthesized assets, and "
                             f"example logs (default: {DEFAULT_OUTPUT_DIR})")
    parser.add_argument("--verbose", action="store_true")

    return parser.parse_args()


def result_line(result, extra: str = "", token: str = "") -> str:
    """
    Format one model's console line.

    The error is collapsed onto one line and truncated so a run of many models
    stays scannable. The full text follows in the end-of-run summary and in
    the reports.

    Args:
        result (ModelResult): The completed validation record.
        extra (str): Text inserted before the error note (e.g. hardware).
        token (str): Token to scrub from the error text.

    Returns:
        line (str): The formatted line.
    """

    error = " ".join(result.error.split())
    if len(error) > CONSOLE_ERROR_CHARS:
        error = error[:CONSOLE_ERROR_CHARS] + " […]"
    note = f" - {error}" if error else ""

    return (f"{status_emoji(result)} {result.status:4s} {result.target} "
            f"({result.duration}s){extra}{scrub(note, token)}")


def resolve_example_dir(name: str) -> Path:
    """
    Resolve one --local-examples argument to an example directory.

    An existing path is used as given, so an example outside the pyharp tree
    can still be validated. Otherwise the name is read the way model names are
    read elsewhere: "pitch_shifter" and "examples/pitch_shifter" both mean that
    example under pyharp/examples/.

    Args:
        name (str): A directory path, bare example name, or "examples/<name>".

    Returns:
        app_dir (Path): The directory expected to contain app.py.
    """

    path = Path(name)

    return path if path.exists() else DEFAULT_EXAMPLES_DIR / path.name


def validate_examples(opts: argparse.Namespace, config: dict,
                      exclusions: Exclusions, assets: Assets) -> list:
    """
    Run the examples tier: launch and validate each local pyharp example.

    Examples run sequentially so concurrent model loads cannot exhaust the
    machine's memory.

    Args:
        opts (argparse.Namespace): Parsed command-line options.
        config (dict): Parsed configuration.
        exclusions (Exclusions): Models to leave out.
        assets (Assets): Synthesized input files.

    Returns:
        results (list): ModelResult objects, or None when no examples exist.
    """

    overrides = config.get("overrides", {})
    # Examples named on the command line are exempt from a config exclusion
    named = bool(opts.local_examples)

    if opts.local_examples:
        app_dirs = [resolve_example_dir(name) for name in opts.local_examples]
        # A named example that does not exist is a mistake in the invocation,
        # not a broken model, so report it as such rather than failing it
        unresolved = [d for d in app_dirs if not (d / "app.py").exists()]
        if unresolved:
            print("ERROR: no app.py in " +
                  ", ".join(str(d) for d in unresolved), file=sys.stderr)
            return None
    elif DEFAULT_EXAMPLES_DIR.is_dir():
        app_dirs = sorted(d for d in DEFAULT_EXAMPLES_DIR.iterdir()
                          if (d / "app.py").exists())
    else:
        print(f"ERROR: {DEFAULT_EXAMPLES_DIR} does not exist. Check out the "
              f"pyharp submodule (git submodule update --init)", file=sys.stderr)
        return None
    app_dirs = [d for d in app_dirs
                if not exclusions.excludes(f"examples/{d.name}", named)]

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


def validate_spaces(opts: argparse.Namespace, config: dict,
                    exclusions: Exclusions, assets: Assets, token: str) -> list:
    """
    Run the spaces tier: validate remote Hugging Face Spaces concurrently.

    Args:
        opts (argparse.Namespace): Parsed command-line options.
        config (dict): Parsed configuration.
        exclusions (Exclusions): Models to leave out.
        assets (Assets): Synthesized input files.
        token (str): Hugging Face access token.

    Returns:
        results (list): ModelResult objects, or None when no spaces exist.
    """

    from huggingface_hub import HfApi

    overrides = config.get("overrides", {})
    api = HfApi(token=token)

    if opts.spaces:
        # Spaces named here are exempt from a config exclusion
        space_ids = [s for s in (qualify(s, opts.org) for s in opts.spaces)
                     if not exclusions.excludes(s, named=True)]
    else:
        space_ids = discover_spaces(api, opts.org, config, exclusions)

    if not space_ids:
        print(f"ERROR: no spaces found for org '{opts.org}'", file=sys.stderr)
        return None

    tracker = ZeroGPUTracker()
    quota_exhausted = threading.Event()
    zerogpu_limiter = threading.Semaphore(max(1, opts.zerogpu_workers))
    print(f"Validating {len(space_ids)} spaces with {opts.workers} workers "
          f"({opts.zerogpu_workers} concurrent on ZeroGPU, "
          f"process test: {'OFF' if opts.load_only else 'ON'})\n")

    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=opts.workers) as pool:
        futures = [pool.submit(test_space, sid, token, assets, opts,
                               overrides.get(sid, {}), quota_exhausted, zerogpu_limiter)
                   for sid in space_ids]
        for future in concurrent.futures.as_completed(futures):
            r = future.result()
            results.append(r)
            # Show the hardware for every model. ZeroGPU work only accrues for
            # ZeroGPU models. Count every /process call that reached the GPU
            # (including retries, which reserve again) and the wall time of the
            # cases that made them. A queued or input-skipped case never ran
            info = r.hardware or "?"
            if is_zerogpu(r.hardware):
                ran = [c for c in r.cases if c.gpu_calls]
                tracker.add(sum(c.gpu_calls for c in ran),
                            sum(c.duration for c in ran))
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
    # Resolved so a case's `files:` paths stay relative to the config file
    # itself, whatever directory the run was launched from
    opts.config = opts.config.resolve()
    config = load_config(opts.config)

    examples_tier = opts.local_examples is not None

    try:
        # Checked before anything reads a setting, so a mistake is reported
        # once, names the key as it was written, and costs no model runs
        check_config_keys(config)
        check_synthesized_inputs(config)

        # Only one tier runs, so a bare model name resolves against that tier
        owner = "examples" if examples_tier else opts.org
        config["overrides"] = qualify_keys(config.get("overrides"), owner)
    except ValueError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2

    exclusions = get_excluded(config, opts.exclude, owner)
    # Generic cases applied to every model, on top of its own (see config.yml)
    opts.common_test_cases = config.get("common_test_cases", [])

    # Each run writes to its own timestamped directory so runs never overwrite
    # each other. Assets, logs, and reports all live under it. Local time is
    # used for the directory name (on a UTC CI runner this is naturally UTC).
    run_stamp = time.strftime("%Y-%m-%dT%H-%M-%S", time.localtime())
    opts.output_dir = opts.output_dir / run_stamp
    assets = Assets(opts.output_dir / "assets",
                    config.get("synthesized_inputs"))

    if examples_tier:
        token = ""
        results = validate_examples(opts, config, exclusions, assets)
    else:
        token = get_token(required=True)
        results = validate_spaces(opts, config, exclusions, assets, token)

    if results is None:
        return 2

    # Record how the run was invoked, so the report says what produced it.
    # argv holds no secrets, as the token comes from HF_TOKEN.
    report = write_reports(results, opts.output_dir, " ".join(sys.argv))

    summary = f"\n{report['passed']}/{report['validated']} models passed"
    if report["skipped"]:
        summary += f", {report['skipped']} skipped"
    print(f"{summary}. Reports written to {opts.output_dir}/")

    failed = [r for r in results if r.status == FAIL]
    if failed:
        print("\nFailed models:")
        for r in sorted(failed, key=lambda r: r.target):
            print(f"  - {r.target}: {scrub(r.error, token)}")

    return 1 if failed else 0


if __name__ == "__main__":
    code = main()
    # Hard-exit rather than sys.exit, because gradio_client and timed-out calls
    # can leave non-daemon threads behind. A normal interpreter shutdown would
    # join them, hanging the process after all results are printed. Reports are
    # already flushed to disk at this point.
    sys.stdout.flush()
    sys.stderr.flush()
    os._exit(code)
