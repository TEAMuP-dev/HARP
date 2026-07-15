#!/usr/bin/env python3
"""
HARP model validation.

Validates HARP deployments in two modes, using the same black-box harness:

1. Remote Hugging Face Spaces (default): discovers all Spaces under an
   organization (default: teamup-tech), verifies each is running, exposes the
   HARP gradio endpoints (/controls and /process), and processes test inputs
   end-to-end. ZeroGPU spaces require an authenticated request for GPU quota,
   so a token must be provided via the HF_TOKEN environment variable - never
   on the command line or in the repository.

2. Baseline (--local-examples): launches each app under pyharp/examples/ on
   a local port and runs the identical endpoint tests. The baseline tier
   exercises pyharp itself with no Hugging Face infrastructure in the loop,
   so a failure indicates a pyharp/gradio-level breakage rather than a
   deployment-specific one.

Per-model test cases can be declared in config.yml (see README.md). When a
model has no configured cases, a single "default" case runs with inputs
synthesized automatically from the /controls spec. Models are validated
independently: a failure (or timeout) in one model never stops validation
of the others.

Usage:
    HF_TOKEN=... python model_validation/validate_models.py
    HF_TOKEN=... python model_validation/validate_models.py --spaces teamup-tech/pitch_shifter
    HF_TOKEN=... python model_validation/validate_models.py --skip-process --workers 8
    HF_TOKEN=... python model_validation/validate_models.py --restart-failed
    python model_validation/validate_models.py --local-examples

Exit codes:
    0 - all validated models passed (or were explicitly skipped)
    1 - at least one model failed
    2 - infrastructure/configuration error (bad token, no spaces found, ...)
"""

import argparse
import concurrent.futures
import dataclasses
import json
import math
import os
import struct
import subprocess
import sys
import time
import traceback
import urllib.request
import wave
from pathlib import Path

try:
    import yaml
except ImportError:
    yaml = None

from gradio_client import Client, handle_file


DEFAULT_ORG = "teamup-tech"
SCRIPT_DIR = Path(__file__).parent
DEFAULT_CONFIG = SCRIPT_DIR / "config.yml"
DEFAULT_EXAMPLES_DIR = SCRIPT_DIR.parent / "pyharp" / "examples"

PASS, FAIL, SKIP = "PASS", "FAIL", "SKIP"

# Space runtime stages that indicate a hard failure (no point connecting)
DEAD_STAGES = {"BUILD_ERROR", "RUNTIME_ERROR", "CONFIG_ERROR", "DELETING"}
# Stages that resolve on their own if we wait (or wake on first request)
TRANSIENT_STAGES = {"BUILDING", "RUNNING_BUILDING", "APP_STARTING", "SLEEPING"}

LOCAL_PORT_BASE = 7861


def get_token(required: bool) -> str:
    token = os.environ.get("HF_TOKEN", "").strip()
    if required and not token:
        print("ERROR: HF_TOKEN environment variable is not set.", file=sys.stderr)
        print("Set it locally (export HF_TOKEN=...) or as a GitHub Actions secret.",
              file=sys.stderr)
        sys.exit(2)
    return token


def scrub(text: str, token: str) -> str:
    """Remove the token from any string that might get printed or reported."""
    return text.replace(token, "***HF_TOKEN***") if token else text


# ---------------------------------------------------------------------------
# Synthesized test assets
# ---------------------------------------------------------------------------

def make_test_wav(path: Path, duration: float = 2.0, sr: int = 44100) -> Path:
    """Write a short mono 16-bit sine sweep - a valid input for any audio model."""
    n = int(duration * sr)
    with wave.open(str(path), "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(sr)
        frames = bytearray()
        for i in range(n):
            t = i / sr
            freq = 220.0 + 440.0 * t / duration
            sample = int(0.5 * 32767 * math.sin(2 * math.pi * freq * t))
            frames += struct.pack("<h", sample)
        f.writeframes(bytes(frames))
    return path


def make_test_midi(path: Path) -> Path:
    """Write a minimal standard MIDI file (format 0, two quarter notes)."""
    track_events = bytes([
        0x00, 0xC0, 0x00,               # program change: acoustic grand
        0x00, 0x90, 0x3C, 0x64,         # note on  C4
        0x83, 0x60, 0x80, 0x3C, 0x40,   # note off C4 after 480 ticks
        0x00, 0x90, 0x40, 0x64,         # note on  E4
        0x83, 0x60, 0x80, 0x40, 0x40,   # note off E4 after 480 ticks
        0x00, 0xFF, 0x2F, 0x00,         # end of track
    ])
    header = b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480)
    track = b"MTrk" + struct.pack(">I", len(track_events)) + track_events
    path.write_bytes(header + track)
    return path


AUDIO_EXTS = {".wav", ".mp3", ".flac", ".ogg", ".aif", ".aiff", ".m4a", "audio"}
MIDI_EXTS = {".mid", ".midi"}


class Assets:
    """Synthesized test input files, shared across all model tests."""

    def __init__(self, workdir: Path):
        workdir.mkdir(parents=True, exist_ok=True)
        self.wav = make_test_wav(workdir / "test_input.wav")
        self.midi = make_test_midi(workdir / "test_input.mid")
        self.text = workdir / "test_input.txt"
        self.text.write_text("HARP model validation\n")
        self.json = workdir / "test_input.json"
        self.json.write_text("{}\n")

    def for_file_types(self, file_types: list) -> Path | None:
        types = {str(t).lower() for t in (file_types or [])}
        if not types or types & AUDIO_EXTS:
            return self.wav
        if types & MIDI_EXTS:
            return self.midi
        if ".json" in types:
            return self.json
        if ".txt" in types or ".text" in types:
            return self.text
        return None


# ---------------------------------------------------------------------------
# Input synthesis from the /controls spec + per-model test cases
# ---------------------------------------------------------------------------

def synthesize_default_args(controls: dict, assets: Assets) -> tuple[list, dict]:
    """
    Build the positional argument list for /process from the controls spec.
    Returns (args, missing) where missing maps the label of every input we
    could NOT synthesize to a reason. Such inputs get a None placeholder; a
    test case can still run if its controls/files overrides cover them all.
    """
    args, missing = [], {}
    for spec in controls.get("inputs", []):
        ctype = spec.get("type")
        label = spec.get("label")
        if ctype == "audio_track":
            args.append(handle_file(str(assets.wav)) if spec.get("required", True) else None)
        elif ctype == "midi_track":
            args.append(handle_file(str(assets.midi)) if spec.get("required", True) else None)
        elif ctype == "generic_file":
            path = assets.for_file_types(spec.get("file_types"))
            if path is None and spec.get("required", True):
                missing[label] = f"cannot synthesize input for file_types={spec.get('file_types')}"
            args.append(handle_file(str(path)) if path is not None else None)
        elif ctype in ("slider", "number_box"):
            value = spec.get("value")
            if value is None:
                value = spec.get("minimum", 0)
            args.append(value)
        elif ctype == "text_box":
            args.append(spec.get("value") or "test")
        elif ctype == "toggle":
            args.append(bool(spec.get("value", False)))
        elif ctype == "dropdown":
            value = spec.get("value")
            if value is None:
                choices = spec.get("choices") or []
                if not choices:
                    missing[label] = "dropdown has no choices"
                else:
                    first = choices[0]
                    value = first[1] if isinstance(first, (list, tuple)) and len(first) > 1 else first
            args.append(value)
        else:
            missing[label] = f"unsupported input control type '{ctype}'"
            args.append(None)
    return args, missing


def apply_case(args: list, controls: dict, case: dict, config_dir: Path) -> list:
    """
    Overlay a configured test case onto the default argument list.

    A case may contain:
        controls: {<input label>: <value>}  - override scalar control values
        files:    {<input label>: <path>}   - override track/file inputs
                                              (paths relative to config.yml)
    Raises ValueError if a label does not match any input.
    """
    labels = [spec.get("label") for spec in controls.get("inputs", [])]
    args = list(args)

    for label, value in (case.get("controls") or {}).items():
        if label not in labels:
            raise ValueError(f"test case '{case.get('name')}' references unknown "
                             f"control '{label}' (available: {labels})")
        args[labels.index(label)] = value

    for label, rel_path in (case.get("files") or {}).items():
        if label not in labels:
            raise ValueError(f"test case '{case.get('name')}' references unknown "
                             f"input '{label}' (available: {labels})")
        path = (config_dir / rel_path).resolve()
        if not path.exists():
            raise ValueError(f"test case '{case.get('name')}': file not found: {path}")
        args[labels.index(label)] = handle_file(str(path))

    return args


def validate_outputs(result, controls: dict) -> str | None:
    """Return an error string if the /process outputs look wrong, else None."""
    specs = controls.get("outputs", [])
    outputs = result if isinstance(result, (list, tuple)) else [result]
    if len(specs) > 1 and len(outputs) != len(specs):
        return f"expected {len(specs)} outputs, got {len(outputs)}"
    for spec, out in zip(specs, outputs):
        if out is None:
            return f"output '{spec.get('label')}' is None"
        # gradio_client downloads file outputs and returns local paths
        path = out.get("path") if isinstance(out, dict) and "path" in out else out
        if isinstance(path, str) and os.path.sep in path and os.path.exists(path):
            if os.path.getsize(path) == 0:
                return f"output file for '{spec.get('label')}' is empty"
    return None


# ---------------------------------------------------------------------------
# Results
# ---------------------------------------------------------------------------

@dataclasses.dataclass
class CaseResult:
    name: str
    ok: bool | None = None   # None => skipped
    duration: float = 0.0
    error: str = ""


@dataclasses.dataclass
class ModelResult:
    target: str                       # space id or "local/<example>"
    kind: str = "space"               # "space" | "local"
    status: str = FAIL
    stage: str = ""
    controls_ok: bool = False
    cases: list = dataclasses.field(default_factory=list)
    duration: float = 0.0
    error: str = ""
    model_name: str = ""


def run_with_timeout(fn, timeout: float, what: str):
    """Run fn() in a worker thread; raise TimeoutError if it exceeds timeout."""
    pool = concurrent.futures.ThreadPoolExecutor(max_workers=1)
    future = pool.submit(fn)
    try:
        return future.result(timeout=timeout)
    except concurrent.futures.TimeoutError:
        raise TimeoutError(f"{what} timed out after {int(timeout)}s")
    finally:
        # wait=False: never block on a hung call; the worker thread is leaked,
        # which is acceptable for a test script
        pool.shutdown(wait=False)


# ---------------------------------------------------------------------------
# Shared endpoint tests (identical for remote spaces and local examples)
# ---------------------------------------------------------------------------

def run_endpoint_tests(client: Client, result: ModelResult, assets: Assets,
                       overrides: dict, opts: argparse.Namespace) -> ModelResult:
    """Verify /controls and run every configured /process test case."""
    process_timeout = overrides.get("process_timeout", opts.process_timeout)
    skip_process = opts.skip_process or overrides.get("skip_process", False)
    config_dir = opts.config.parent.resolve()

    endpoints = client.view_api(return_format="dict", print_info=False) or {}
    named = endpoints.get("named_endpoints", {})
    if "/controls" not in named or "/process" not in named:
        result.error = (f"missing HARP endpoints (found: {sorted(named)}); "
                        f"deployment may use an outdated pyharp")
        return result

    controls = run_with_timeout(
        lambda: client.predict(api_name="/controls"), 120, "/controls")
    if not isinstance(controls, dict) or "card" not in controls or "inputs" not in controls:
        result.error = f"/controls returned malformed data: {str(controls)[:200]}"
        return result
    result.controls_ok = True
    result.model_name = controls.get("card", {}).get("name", "")

    if skip_process:
        result.status = PASS
        return result

    default_args, missing = synthesize_default_args(controls, assets)
    cases = overrides.get("test_cases") or [{"name": "default"}]

    for case in cases:
        case_result = CaseResult(name=case.get("name", "unnamed"))
        result.cases.append(case_result)
        case_start = time.time()
        try:
            # Inputs we couldn't synthesize are fine if this case supplies them
            supplied = set(case.get("controls") or {}) | set(case.get("files") or {})
            unsatisfied = {k: v for k, v in missing.items() if k not in supplied}
            if unsatisfied:
                case_result.error = "skipped: " + "; ".join(
                    f"'{k}': {v}" for k, v in unsatisfied.items())
                continue
            args = apply_case(default_args, controls, case, config_dir)
            job = client.submit(*args, api_name="/process")
            output = job.result(timeout=case.get("process_timeout", process_timeout))
            error = validate_outputs(output, controls)
            if error:
                case_result.ok = False
                case_result.error = f"/process output invalid: {error}"
            else:
                case_result.ok = True
        except Exception as exc:  # noqa: BLE001 - any exception fails the case
            case_result.ok = False
            case_result.error = f"{type(exc).__name__}: {exc}"
        finally:
            case_result.duration = round(time.time() - case_start, 1)

    if any(c.ok is False for c in result.cases):
        failed = [c.name for c in result.cases if c.ok is False]
        result.error = "; ".join(
            f"[{c.name}] {c.error}" for c in result.cases if c.ok is False)
        result.status = FAIL
    else:
        result.status = PASS
    return result


# ---------------------------------------------------------------------------
# Remote space tests
# ---------------------------------------------------------------------------

def wait_for_stage(api, space_id: str, deadline: float) -> str:
    """Poll runtime stage until RUNNING, a dead stage, or the deadline."""
    stage = api.get_space_runtime(space_id).stage
    while stage in TRANSIENT_STAGES - {"SLEEPING"} and time.time() < deadline:
        time.sleep(10)
        stage = api.get_space_runtime(space_id).stage
    return stage


def test_space(space_id: str, token: str, assets: Assets,
               opts: argparse.Namespace, overrides: dict) -> ModelResult:
    from huggingface_hub import HfApi

    result = ModelResult(target=space_id, kind="space")
    start = time.time()
    api = HfApi(token=token)
    connect_timeout = overrides.get("connect_timeout", opts.connect_timeout)

    try:
        deadline = time.time() + connect_timeout
        stage = wait_for_stage(api, space_id, deadline)
        result.stage = stage

        if stage in DEAD_STAGES or stage in ("STOPPED", "PAUSED"):
            if opts.restart_failed and stage != "DELETING":
                print(f"  [{space_id}] stage={stage}, requesting restart...")
                api.restart_space(space_id)
                stage = wait_for_stage(api, space_id, time.time() + connect_timeout)
                result.stage = stage
            if stage not in ("RUNNING", "SLEEPING"):
                result.error = f"space is not running (stage={stage})"
                return result

        # Connect (this wakes sleeping spaces; retry through the wake-up)
        client, last_exc = None, None
        deadline = time.time() + connect_timeout
        while time.time() < deadline:
            try:
                client = run_with_timeout(
                    lambda: Client(space_id, hf_token=token, verbose=False),
                    max(10.0, deadline - time.time()), "connect")
                break
            except Exception as exc:  # noqa: BLE001 - space may still be waking
                last_exc = exc
                time.sleep(15)
        if client is None:
            raise RuntimeError(f"could not connect: {last_exc}")

        return run_endpoint_tests(client, result, assets, overrides, opts)

    except Exception as exc:  # noqa: BLE001
        result.error = scrub(f"{type(exc).__name__}: {exc}", token)
        if opts.verbose:
            traceback.print_exc()
        return result
    finally:
        result.duration = round(time.time() - start, 1)


# ---------------------------------------------------------------------------
# Baseline: local pyharp example tests
# ---------------------------------------------------------------------------

def wait_for_local_server(port: int, proc: subprocess.Popen, timeout: float) -> None:
    deadline = time.time() + timeout
    url = f"http://127.0.0.1:{port}/config"
    while time.time() < deadline:
        if proc.poll() is not None:
            raise RuntimeError(f"app exited early with code {proc.returncode}")
        try:
            with urllib.request.urlopen(url, timeout=5):
                return
        except Exception:  # noqa: BLE001 - server not up yet
            time.sleep(2)
    raise TimeoutError(f"local app did not become ready within {int(timeout)}s")


def test_local_example(app_dir: Path, port: int, assets: Assets,
                       opts: argparse.Namespace, overrides: dict) -> ModelResult:
    target = f"local/{app_dir.name}"
    result = ModelResult(target=target, kind="local", stage="LOCAL")
    start = time.time()

    env = os.environ.copy()
    env["GRADIO_SERVER_NAME"] = "127.0.0.1"
    env["GRADIO_SERVER_PORT"] = str(port)
    env.pop("HF_TOKEN", None)  # local examples must not need credentials

    log_path = opts.output_dir / f"{app_dir.name}.log"
    log_path.parent.mkdir(parents=True, exist_ok=True)
    proc = None
    try:
        with open(log_path, "w") as log:
            proc = subprocess.Popen(
                [sys.executable, "app.py"], cwd=app_dir, env=env,
                stdout=log, stderr=subprocess.STDOUT)
        wait_for_local_server(port, proc, overrides.get(
            "connect_timeout", opts.connect_timeout))
        client = Client(f"http://127.0.0.1:{port}", verbose=False)
        return run_endpoint_tests(client, result, assets, overrides, opts)
    except Exception as exc:  # noqa: BLE001
        result.error = f"{type(exc).__name__}: {exc} (see {log_path.name})"
        if opts.verbose:
            traceback.print_exc()
        return result
    finally:
        result.duration = round(time.time() - start, 1)
        if proc is not None and proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=15)
            except subprocess.TimeoutExpired:
                proc.kill()


# ---------------------------------------------------------------------------
# Discovery, config, reporting
# ---------------------------------------------------------------------------

def load_config(path: Path) -> dict:
    if not path.exists():
        return {}
    if yaml is None:
        print(f"WARNING: PyYAML not installed; ignoring {path}", file=sys.stderr)
        return {}
    return yaml.safe_load(path.read_text()) or {}


def get_excluded(config: dict, opts: argparse.Namespace) -> set:
    """Models excluded from validation: config `exclude` list + --exclude."""
    return set(config.get("exclude", [])) | set(opts.exclude or [])


def discover_spaces(api, org: str, config: dict, excluded: set) -> list[str]:
    spaces = [s.id for s in api.list_spaces(author=org)]
    spaces += [s for s in config.get("include_extra", []) if s not in spaces]
    return sorted(s for s in spaces if s not in excluded)


def status_emoji(r: ModelResult) -> str:
    return {"PASS": "✅", "FAIL": "❌", "SKIP": "⏭️"}[r.status]


def write_reports(results: list[ModelResult], out_dir: Path, label: str) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)

    payload = {
        "label": label,
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "total": len(results),
        "passed": sum(r.status == PASS for r in results),
        "failed": sum(r.status == FAIL for r in results),
        "results": [dataclasses.asdict(r) for r in results],
    }
    (out_dir / "report.json").write_text(json.dumps(payload, indent=2))

    lines = [
        f"# HARP Model Validation Report - {label}",
        "",
        f"**{payload['passed']}/{payload['total']} models passed** ({payload['timestamp']})",
        "",
        "| Model | Status | Stage | Controls | Cases | Time (s) | Detail |",
        "|---|---|---|---|---|---|---|",
    ]
    for r in sorted(results, key=lambda r: (r.status != FAIL, r.target)):
        if r.cases:
            cases = ", ".join(
                f"{c.name} {'✅' if c.ok else '⏭️' if c.ok is None else '❌'}"
                for c in r.cases)
        else:
            cases = "—"
        link = (f"[{r.target}](https://huggingface.co/spaces/{r.target})"
                if r.kind == "space" else f"`{r.target}`")
        detail = r.error.replace("|", "\\|")[:300] if r.error else ""
        lines.append(f"| {link} | {status_emoji(r)} {r.status} | {r.stage} "
                     f"| {'✅' if r.controls_ok else '❌'} | {cases} "
                     f"| {r.duration} | {detail} |")
    (out_dir / "report.md").write_text("\n".join(lines) + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate HARP model deployments.")
    parser.add_argument("--org", default=DEFAULT_ORG, help="HF organization to scan")
    parser.add_argument("--spaces", nargs="*", default=None,
                        help="Explicit space ids to validate (skips discovery)")
    parser.add_argument("--exclude", nargs="*", default=None, metavar="MODEL",
                        help="Models to exclude from validation (space ids, or "
                             "local/<example-dir>); merged with the config "
                             "`exclude` list")
    parser.add_argument("--local-examples", nargs="*", default=None, metavar="DIR",
                        help="Validate local pyharp example apps instead of remote "
                             "spaces (the baseline tier). With no DIRs given, tests "
                             "every app under pyharp/examples/.")
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG,
                        help="Optional YAML config (excludes, per-model overrides/cases)")
    parser.add_argument("--skip-process", action="store_true",
                        help="Only verify availability and /controls, do not run inference")
    parser.add_argument("--restart-failed", action="store_true",
                        help="Attempt to restart spaces found in an error/stopped state "
                             "(requires a token with write access)")
    parser.add_argument("--workers", type=int, default=4,
                        help="Number of spaces to test concurrently (remote mode only)")
    parser.add_argument("--connect-timeout", type=float, default=420,
                        help="Seconds to wait for a deployment to build/wake/start")
    parser.add_argument("--process-timeout", type=float, default=600,
                        help="Seconds to wait for /process (includes ZeroGPU queue time)")
    parser.add_argument("--output-dir", type=Path, default=Path("reports"))
    parser.add_argument("--verbose", action="store_true")
    opts = parser.parse_args()

    config = load_config(opts.config)
    overrides = config.get("overrides", {})
    excluded = get_excluded(config, opts)
    assets = Assets(opts.output_dir / "assets")
    results = []

    if opts.local_examples is not None:
        # ---- Baseline mode: local pyharp examples (run sequentially) ----
        if opts.local_examples:
            app_dirs = [Path(d) for d in opts.local_examples]
        else:
            app_dirs = sorted(d for d in DEFAULT_EXAMPLES_DIR.iterdir()
                              if (d / "app.py").exists())
        app_dirs = [d for d in app_dirs if f"local/{d.name}" not in excluded]
        if not app_dirs:
            print("ERROR: no local examples found", file=sys.stderr)
            return 2
        print(f"Validating {len(app_dirs)} local pyharp examples (baseline)\n")
        for i, app_dir in enumerate(app_dirs):
            r = test_local_example(app_dir, LOCAL_PORT_BASE + i, assets, opts,
                                   overrides.get(f"local/{app_dir.name}", {}))
            results.append(r)
            note = f" - {r.error}" if r.error else ""
            print(f"{status_emoji(r)} {r.status:4s} {r.target} ({r.duration}s){note}")
        label = "baseline (local pyharp examples)"
        token = ""
    else:
        # ---- Remote mode: Hugging Face Spaces ----
        from huggingface_hub import HfApi
        token = get_token(required=True)
        api = HfApi(token=token)
        if opts.spaces:
            space_ids = [s for s in opts.spaces if s not in excluded]
        else:
            space_ids = discover_spaces(api, opts.org, config, excluded)
        if not space_ids:
            print(f"ERROR: no spaces found for org '{opts.org}'", file=sys.stderr)
            return 2
        print(f"Validating {len(space_ids)} spaces with {opts.workers} workers "
              f"(process test: {'OFF' if opts.skip_process else 'ON'})\n")
        with concurrent.futures.ThreadPoolExecutor(max_workers=opts.workers) as pool:
            futures = {
                pool.submit(test_space, sid, token, assets, opts,
                            overrides.get(sid, {})): sid
                for sid in space_ids
            }
            for future in concurrent.futures.as_completed(futures):
                r = future.result()
                results.append(r)
                note = f" - {r.error}" if r.error else ""
                print(f"{status_emoji(r)} {r.status:4s} {r.target} "
                      f"({r.duration}s){scrub(note, token)}")
        label = f"{opts.org} spaces"

    write_reports(results, opts.output_dir, label)

    failed = [r for r in results if r.status == FAIL]
    print(f"\n{len(results) - len(failed)}/{len(results)} models passed. "
          f"Reports written to {opts.output_dir}/")
    if failed:
        print("\nFailed models:")
        for r in sorted(failed, key=lambda r: r.target):
            print(f"  - {r.target}: {scrub(r.error, token)}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
