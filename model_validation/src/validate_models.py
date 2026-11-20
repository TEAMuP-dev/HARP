#!/usr/bin/env python3
"""
HARP model validation - command-line entry point.

Parses the options, runs one of the three tiers, and writes the reports:

1. Spaces (default): every Space under an organization, validated concurrently
   by tiers/spaces.py. Requires a Hugging Face token, read only from the
   HF_TOKEN environment variable so it never appears in argv or in the
   repository.

2. Examples (--local-examples): the pyharp example apps, launched locally and
   validated one at a time by tiers/examples.py. Needs no token, and exercises
   pyharp itself with no Hugging Face infrastructure in the loop.

3. Lint (--lint): every Space's files, read from the Hub and checked against
   docs/DEPLOYMENT.md by tiers/lint.py, without running anything. Needs uv to
   resolve their requirements, and the token unless the Spaces are named and
   public.

The first two run the same checks, from endpoint/harness.py. Which models to
validate and what to assert about their outputs is configured in config.yml.
README.md is the guide to writing that.

Usage:
    HF_TOKEN=... python model_validation/src/validate_models.py
    HF_TOKEN=... python model_validation/src/validate_models.py --spaces pitch_shifter
    HF_TOKEN=... python model_validation/src/validate_models.py --skip-zerogpu
    HF_TOKEN=... python model_validation/src/validate_models.py --lint
    python model_validation/src/validate_models.py --lint --spaces <owner>/<name>
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

from media.assets import Assets, check_synthesized_inputs
from quota import ZeroGPUTracker
from results import FAIL, status_emoji, write_reports
from tiers.examples import test_local_example
from tiers.lint import ERROR, check_lint_exceptions, find_uv, lint_space
from tiers.spaces import test_space
from utils import (
    Exclusions,
    check_config_keys,
    check_config_values,
    check_model_names,
    check_token,
    discover_spaces,
    get_excluded,
    get_token,
    load_config,
    qualify,
    qualify_keys,
    scrub,
)


DEFAULT_ORG = "teamup-tech"
SCRIPT_DIR = Path(__file__).parent  # model_validation/src
MODEL_VALIDATION_DIR = SCRIPT_DIR.parent  # model_validation
REPO_ROOT = MODEL_VALIDATION_DIR.parent
DEFAULT_CONFIG = MODEL_VALIDATION_DIR / "config.yml"
DEFAULT_EXAMPLES_DIR = REPO_ROOT / "pyharp" / "examples"
DEFAULT_OUTPUT_DIR = MODEL_VALIDATION_DIR / "reports"

LOCAL_PORT_BASE = 7861

# Cap on the error text shown on the console line of a model (see result_line)
CONSOLE_ERROR_CHARS = 200


def parse_args() -> argparse.Namespace:
    """
    Define and parse the command-line interface.

    Returns:
        opts (argparse.Namespace): Parsed options.
    """

    parser = argparse.ArgumentParser(description="Validate HARP model deployments.")
    parser.add_argument(
        "--org",
        default=DEFAULT_ORG,
        help=f"HF organization whose spaces are discovered, and "
        f"the owner a bare space name is taken to have "
        f"(default: {DEFAULT_ORG})",
    )
    parser.add_argument(
        "--spaces",
        nargs="*",
        default=None,
        help="Explicit spaces to validate, skipping discovery. A "
        "bare name is taken to be in --org. Give 'owner/name' "
        "to reach another organization",
    )
    parser.add_argument(
        "--exclude",
        nargs="*",
        default=None,
        metavar="MODEL",
        help="Models to exclude, in either tier. A bare name means "
        "the model in the tier being run. Qualify it "
        "('owner/name' or 'examples/<dir>') to pin it to one. "
        "Merged with the config `exclude` list",
    )
    parser.add_argument(
        "--local-examples",
        nargs="*",
        default=None,
        metavar="DIR",
        help="Validate local pyharp example apps instead of remote "
        "spaces. Name them by directory ('pitch_shifter' or "
        "'examples/pitch_shifter'), or give a path to an example "
        "kept elsewhere. With none given, every app under "
        "pyharp/examples/ is validated",
    )
    parser.add_argument(
        "--lint",
        action="store_true",
        help="Check each Space's files against docs/DEPLOYMENT.md "
        "instead of running it. Spends no ZeroGPU allowance and "
        "wakes nothing. Selects Spaces like a spaces run",
    )
    parser.add_argument(
        "--config",
        type=Path,
        default=DEFAULT_CONFIG,
        help="Optional YAML config (excludes, per-model overrides/cases)",
    )
    parser.add_argument(
        "--load-only",
        action="store_true",
        help="Only verify availability and /controls, do not run inference",
    )
    parser.add_argument(
        "--cases",
        nargs="+",
        default=None,
        metavar="NAME",
        help="Run only the test cases with these names, a common case "
        "named with or without its 'common:' prefix, e.g. to run "
        "the refusals alone, which spend no ZeroGPU allowance on a "
        "model refusing as it should",
    )
    parser.add_argument(
        "--skip-zerogpu",
        action="store_true",
        help="Skip models on ZeroGPU hardware, to avoid spending the shared ZeroGPU allowance",
    )
    parser.add_argument(
        "--restart-failed",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Attempt to restart spaces found in an error or stopped "
        "state. Enabled by default, and requires a token with "
        "write access. Disable with --no-restart-failed",
    )
    parser.add_argument(
        "--workers",
        type=int,
        default=4,
        help="Number of spaces to validate concurrently (spaces tier only)",
    )
    parser.add_argument(
        "--zerogpu-workers",
        type=int,
        default=1,
        help="How many ZeroGPU models may make GPU calls at once. "
        "Each concurrent call reserves its declared duration, "
        "so 1 (the default) keeps overlapping reservations "
        "from tying up the allowance",
    )
    parser.add_argument(
        "--connect-timeout",
        type=float,
        default=420,
        help="Seconds to wait for a deployment to build, wake, or start (default: 420)",
    )
    parser.add_argument(
        "--process-timeout",
        type=float,
        default=600,
        help="Seconds to wait for /process on non-ZeroGPU models (default: 600)",
    )
    parser.add_argument(
        "--zerogpu-process-timeout",
        type=float,
        default=120,
        help="Seconds allowed for /process EXECUTION on ZeroGPU "
        "models once they leave the queue, with the queue "
        "wait bounded separately by --connect-timeout "
        "(default: 120)",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_OUTPUT_DIR,
        help=f"Directory for reports, synthesized assets, and "
        f"example logs (default: {DEFAULT_OUTPUT_DIR})",
    )
    parser.add_argument("--verbose", action="store_true")

    return parser.parse_args()


def result_line(result, extra: str = "", token: str = "") -> str:
    """
    Format the console line of one model.

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

    return (
        f"{status_emoji(result)} {result.status:4s} {result.target} "
        f"({result.duration}s){extra}{scrub(note, token)}"
    )


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


def report_unused_overrides(overrides: dict, selected: list, exclusions: Exclusions) -> None:
    """
    Warn about `overrides` entries that name no model the run found.

    A Space renamed or taken down leaves its entry applying to nothing, and
    every setting under it, test cases included, then goes unused in silence.
    Only a run that discovered its models can tell, since one given `--spaces`
    or `--local-examples` leaves the rest out on purpose.

    Args:
        overrides (dict): The `overrides` block, keyed by qualified name.
        selected (list): The qualified names of the models the run covers.
        exclusions (Exclusions): Models left out, whose entries are expected
            to apply to nothing.
    """

    unused = sorted(
        model for model in overrides if model not in selected and not exclusions.excludes(model)
    )

    if unused:
        print(
            f"WARNING: config.yml configures {', '.join(unused)}, which the run did not "
            f"find, so those settings apply to nothing",
            file=sys.stderr,
        )


def validate_examples(
    opts: argparse.Namespace, config: dict, exclusions: Exclusions, assets: Assets
) -> list:
    """
    Run the examples tier: launch and validate each local pyharp example.

    Examples run sequentially so concurrent model loads cannot exhaust the
    memory of the machine.

    Args:
        opts (argparse.Namespace): Parsed command-line options.
        config (dict): Parsed configuration.
        exclusions (Exclusions): Models to exclude.
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
            print("ERROR: no app.py in " + ", ".join(str(d) for d in unresolved), file=sys.stderr)
            return None
    elif DEFAULT_EXAMPLES_DIR.is_dir():
        app_dirs = sorted(d for d in DEFAULT_EXAMPLES_DIR.iterdir() if (d / "app.py").exists())
    else:
        print(
            f"ERROR: {DEFAULT_EXAMPLES_DIR} does not exist. Check out the "
            f"pyharp submodule (git submodule update --init)",
            file=sys.stderr,
        )
        return None
    app_dirs = [d for d in app_dirs if not exclusions.excludes(f"examples/{d.name}", named)]

    if not app_dirs:
        print("ERROR: no local examples found", file=sys.stderr)
        return None
    if not named:
        report_unused_overrides(overrides, [f"examples/{d.name}" for d in app_dirs], exclusions)

    print(f"Validating {len(app_dirs)} pyharp examples\n")

    results = []
    for i, app_dir in enumerate(app_dirs):
        r = test_local_example(
            app_dir,
            LOCAL_PORT_BASE + i,
            assets,
            opts,
            overrides.get(f"examples/{app_dir.name}", {}),
        )
        results.append(r)
        print(result_line(r))

    return results


def select_spaces(api, opts: argparse.Namespace, config: dict, exclusions: Exclusions) -> list:
    """
    The Spaces a spaces or lint run covers: those named, or else those found.

    Args:
        api (HfApi): Authenticated Hugging Face API client.
        opts (argparse.Namespace): Parsed command-line options.
        config (dict): Parsed configuration.
        exclusions (Exclusions): Models to exclude.

    Returns:
        space_ids (list): Qualified space ids, possibly empty.
    """

    if opts.spaces:
        # Spaces named here are exempt from a config exclusion
        return [
            s
            for s in (qualify(s, opts.org) for s in opts.spaces)
            if not exclusions.excludes(s, named=True)
        ]

    space_ids = discover_spaces(api, opts.org, config, exclusions)
    report_unused_overrides(config.get("overrides", {}), space_ids, exclusions)

    return space_ids


def space_owners(opts: argparse.Namespace) -> list:
    """
    The organizations and accounts whose Spaces a spaces or lint run touches.

    Args:
        opts (argparse.Namespace): Parsed command-line options.

    Returns:
        owners (list): The owners of the Spaces named, or else the organization
            whose Spaces are found.
    """

    if opts.spaces:
        return sorted({qualify(s, opts.org).split("/")[0] for s in opts.spaces})

    return [opts.org]


def validate_lint(
    opts: argparse.Namespace, config: dict, exclusions: Exclusions, token: str
) -> list:
    """
    Run the lint tier: check each Space's files against docs/DEPLOYMENT.md.

    Args:
        opts (argparse.Namespace): Parsed command-line options.
        config (dict): Parsed configuration.
        exclusions (Exclusions): Models to exclude.
        token (str): Hugging Face access token, or an empty string to read
            public Spaces without one.

    Returns:
        results (list): ModelResult objects, or None when the run cannot
            proceed.
    """

    from huggingface_hub import HfApi

    uv = find_uv()
    if uv is None:
        print(
            "ERROR: the lint resolves each Space's requirements with uv, which is not "
            "installed. Run pip install -r model_validation/requirements.txt",
            file=sys.stderr,
        )
        return None

    overrides = config.get("overrides", {})
    # False asks for no token at all, where None would fall back to a saved login
    api = HfApi(token=token or False)
    space_ids = select_spaces(api, opts, config, exclusions)

    if not space_ids:
        print(f"ERROR: no spaces found for org '{opts.org}'", file=sys.stderr)
        return None

    print(f"Linting {len(space_ids)} spaces with {opts.workers} workers\n")

    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=opts.workers) as pool:
        futures = [
            pool.submit(lint_space, api, sid, overrides.get(sid, {}), uv) for sid in space_ids
        ]
        for future in concurrent.futures.as_completed(futures):
            r = future.result()
            results.append(r)
            warnings = sum(finding.severity != ERROR for finding in r.cases)
            note = f" | {warnings} warning{'s' if warnings != 1 else ''}" if warnings else ""
            print(result_line(r, f" [{r.hardware or '?'}{note}]", token))

    return results


def validate_spaces(
    opts: argparse.Namespace, config: dict, exclusions: Exclusions, assets: Assets, token: str
) -> list:
    """
    Run the spaces tier: validate remote Hugging Face Spaces concurrently.

    Args:
        opts (argparse.Namespace): Parsed command-line options.
        config (dict): Parsed configuration.
        exclusions (Exclusions): Models to exclude.
        assets (Assets): Synthesized input files.
        token (str): Hugging Face access token.

    Returns:
        results (list): ModelResult objects, or None when no spaces exist.
    """

    from huggingface_hub import HfApi

    overrides = config.get("overrides", {})
    api = HfApi(token=token)
    space_ids = select_spaces(api, opts, config, exclusions)

    if not space_ids:
        print(f"ERROR: no spaces found for org '{opts.org}'", file=sys.stderr)
        return None

    tracker = ZeroGPUTracker()
    quota_exhausted = threading.Event()
    zerogpu_limiter = threading.Semaphore(max(1, opts.zerogpu_workers))
    print(
        f"Validating {len(space_ids)} spaces with {opts.workers} workers "
        f"({opts.zerogpu_workers} concurrent on ZeroGPU, "
        f"process test: {'OFF' if opts.load_only else 'ON'})\n"
    )

    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=opts.workers) as pool:
        futures = [
            pool.submit(
                test_space,
                sid,
                token,
                assets,
                opts,
                overrides.get(sid, {}),
                quota_exhausted,
                zerogpu_limiter,
            )
            for sid in space_ids
        ]
        for future in concurrent.futures.as_completed(futures):
            r = future.result()
            results.append(r)
            # Show the hardware for every model. ZeroGPU work only accrues for
            # ZeroGPU models. Count every /process call that reached the GPU
            # (including retries, which reserve again) and the wall time of the
            # cases that made them. A queued or input-skipped case never ran
            info = r.hardware or "?"
            if r.zerogpu:
                ran = [c for c in r.cases if c.gpu_calls]
                tracker.add(sum(c.gpu_calls for c in ran), sum(c.duration for c in ran))
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
    # Resolved so the `files:` paths of a case stay relative to the config file
    # itself, whatever directory the run was launched from
    opts.config = opts.config.resolve()

    examples_tier = opts.local_examples is not None
    if examples_tier and opts.lint:
        print(
            "ERROR: --lint checks Spaces, so it does not combine with --local-examples",
            file=sys.stderr,
        )
        return 2

    try:
        config = load_config(opts.config)

        # Checked before anything reads a setting, so a mistake is reported
        # once, names the key as it was written, and costs no model runs
        check_config_keys(config)
        check_config_values(config)
        check_lint_exceptions(config)
        check_model_names(config, opts.exclude, opts.spaces)
        check_synthesized_inputs(config, opts.config.parent)

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
    assets = Assets(
        opts.output_dir / "assets", config.get("synthesized_inputs"), opts.config.parent
    )

    if examples_tier:
        token = ""
        results = validate_examples(opts, config, exclusions, assets)
    else:
        # The lint only reads, and anyone can read a public Space, so a lint of
        # named Spaces goes ahead without a token. A run that finds its Spaces
        # needs one, since the private Spaces of the organization would
        # otherwise be left out in silence.
        token = get_token(required=not (opts.lint and opts.spaces))

        if not token:
            print("HF_TOKEN is not set, so only public Spaces can be read.")
        # Checked once here rather than discovered one failing space at a time.
        # The lint only reads, so it never needs to restart anything.
        elif not check_token(token, space_owners(opts), opts.restart_failed and not opts.lint):
            return 2

        results = (
            validate_lint(opts, config, exclusions, token)
            if opts.lint
            else validate_spaces(opts, config, exclusions, assets, token)
        )

    if results is None:
        return 2

    # Record how the run was invoked, so the report says what produced it.
    # argv holds no secrets, as the token comes from HF_TOKEN.
    report = write_reports(results, opts.output_dir, " ".join(sys.argv))

    noun = "spaces" if opts.lint else "models"
    summary = f"\n{report['passed']}/{report['validated']} {noun} passed"
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
