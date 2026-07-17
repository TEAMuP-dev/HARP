# HARP Model Validation

Automated validation of HARP model deployments. Verifies that each deployment
is reachable, exposes the HARP gradio endpoints (`/controls`, `/process`),
and can actually process test inputs end-to-end — the same interactions the
HARP client performs, driven headlessly.

## Overview

Two tiers share the same harness:

| Tier | Command | What it validates | What a failure means |
|---|---|---|---|
| **Examples** | `--local-examples` | The apps under `pyharp/examples/`, launched locally | pyharp or gradio itself is broken |
| **Spaces** | (default) | Every Hugging Face Space under `teamup-tech` | That specific deployment is broken (build error, HF platform change, dependency drift, ...) |

Key behaviors:

- **Models are validated independently.** Each model (and each test case
  within it) runs inside its own error boundary, so a crash, hang, or
  timeout in one model never stops validation of the others. All results are
  collected into a single report and the process exits non-zero only after
  everything has run.
- **Crashed/stopped spaces are restarted automatically** (sleeping spaces
  are woken simply by connecting). Pass `--no-restart-failed` to disable;
  restarting requires a token with write access.
- **ZeroGPU quota is reported** at the start of a spaces run and after every
  model, so it is easy to tell if and when quota will be exceeded mid-run.
- A daily GitHub Action
  ([model_validation.yml](../.github/workflows/model_validation.yml)) runs
  both tiers at 06:30 UTC. Model failures do **not** turn the run red (no
  notification emails while deployments stabilize) — results live in the
  run summary and report artifacts.

## Code layout

| File | Purpose |
|---|---|
| [validate_models.py](validate_models.py) | Command-line entry point and per-tier orchestration |
| [harness.py](harness.py) | Core endpoint tests; space and local-example drivers |
| [cases.py](cases.py) | Input synthesis, test-case overlay, output validation/inspection |
| [validators.py](validators.py) | Registry of custom output validators (extend this) |
| [assets.py](assets.py) | Synthesized WAV/MIDI/text/JSON test inputs |
| [quota.py](quota.py) | ZeroGPU usage tracking and account quota lookup |
| [results.py](results.py) | Result records and JSON/markdown report generation |
| [utils.py](utils.py) | Token handling, config loading, discovery, timeouts |
| [config.yml](config.yml) | Validation configuration (excludes, per-model test cases) |
| [test_data/](test_data/) | Real input files referenced by test cases |

## Token setup (IMPORTANT — read this)

ZeroGPU spaces require an authenticated request to obtain GPU quota, and
private spaces require read access. Validation reads the token **only** from
the `HF_TOKEN` environment variable.

- **Never** commit a token, paste it in an issue/PR, or pass it as a
  command-line argument (argv is visible in process listings).
- **CI:** add the token as a repository secret named `HF_TOKEN`
  (Settings → Secrets and variables → Actions). GitHub masks secrets in logs.
- **Locally:** `export HF_TOKEN=...` in your shell (consider `read -s` so it
  stays out of shell history).
- Use a [fine-grained token](https://huggingface.co/settings/tokens) with
  write access to the org's spaces (needed for the default auto-restart of
  crashed spaces; read access suffices with `--no-restart-failed`).
- **If a token is ever exposed, rotate it immediately** at
  https://huggingface.co/settings/tokens.

## Running locally

```bash
pip install -r model_validation/requirements.txt

# All spaces in the org (crashed/stopped spaces are restarted by default)
export HF_TOKEN=hf_...   # see token setup above
python model_validation/validate_models.py

# A single space, with verbose errors — the go-to while developing a test case
python model_validation/validate_models.py --spaces teamup-tech/pitch_shifter --verbose

# Exclude specific models (also configurable via `exclude` in config.yml)
python model_validation/validate_models.py --exclude teamup-tech/broken-space

# Availability + /controls only (fast; no inference, no GPU quota used)
python model_validation/validate_models.py --load-only

# Never restart spaces (works with a read-only token)
python model_validation/validate_models.py --no-restart-failed

# Examples tier (needs pyharp + example deps; no token required)
pip install torch torchaudio --index-url https://download.pytorch.org/whl/cpu
pip install -e ./pyharp
python model_validation/validate_models.py --local-examples
```

Reports land in `reports/` as `report.json` (machine-readable) and
`report.md` (human-readable table); local example logs are saved alongside
them. Exit code is `0` when everything passes, `1` on any model failure,
`2` on configuration/infrastructure errors.

## ZeroGPU quota reporting

Space validation prints the quota state at the start of the run and after
every model, on the same line as its pass/fail status:

```
✅ PASS teamup-tech/pitch_shifter (42.1s) [GPU time ~38s/1500s budget | ...]
```

Two signals are combined:

- **GPU time this run** — cumulative `/process` wall time on ZeroGPU-hardware
  spaces only (models on CPU or dedicated hardware do not draw on the
  allowance and are never counted). It is an upper bound on GPU seconds
  consumed, since it includes queue time. Set `zerogpu_budget_seconds` in
  [config.yml](config.yml) (e.g. `1500` for the PRO 25 min/day allowance)
  to show usage against a budget.
- **Account quota** — fetched from `huggingface.co/api/quota` when available.
  Hugging Face has no documented public ZeroGPU quota API, so this part is
  best-effort and silently omitted if the endpoint yields nothing usable.

## Configuring test cases — a walkthrough

Every model automatically gets one **`default`** test case, even with no
configuration at all: inputs are synthesized from the model's `/controls`
spec — a sine-sweep WAV for audio tracks, a two-note MIDI file for MIDI
tracks, and each control's declared default value for sliders, toggles,
dropdowns, number boxes, and text boxes. So configuration is only needed to
go beyond that: pinning specific control values, feeding real audio, or
inspecting outputs more deeply.

### Step 1: find the model's control labels

Test cases reference inputs and outputs by their **label** — the display
name each gradio component was given in the model's `app.py`. Three ways to
find them:

- Open the Space's gradio UI and read the component titles, or click its
  "View Controls" button to see the full spec as JSON;
- Read the model's `app.py` (each component has a `label=...`);
- Reference a wrong label on purpose and run the validator — the error
  message lists every available label.

### Step 2: add the case to config.yml

Cases live under `overrides:` keyed by space id (or `examples/<example-dir>`
for local examples). Each case has a `name` plus any of: `controls` (override
scalar values by label), `files` (substitute input files by label, paths
relative to config.yml — commit real inputs to `test_data/`), and a
per-case `process_timeout`.

```yaml
overrides:
  teamup-tech/pitch_shifter:
    process_timeout: 900          # this model is slow; extend the timeout
    test_cases:
      - name: default             # keep the synthesized case too
      - name: extreme-shift
        controls:
          "Pitch Shift (semitones)": 24
      - name: real-audio
        files:
          "Input Audio A": test_data/short_vocal.wav
```

Note: once `test_cases` is present, **only** the listed cases run — include
`- name: default` to keep the synthesized one.

### Step 3: check the outputs (optional, two levels)

Every case already gets structural checks for free: `/process` must not
error, file outputs must exist and be non-empty, and JSON outputs (e.g. an
optional pyharp `LabelList`) must be well-formed when present (absent/None
is valid, since labels are optional).

**Level 1 — declarative `expect` rules**, simple per-output assertions:

```yaml
      - name: extreme-shift
        controls:
          "Pitch Shift (semitones)": 24
        expect:
          "Output Audio":
            ext: .wav         # downloaded file must have this extension
            min_bytes: 10000  # ... and be at least this many bytes
```

**Level 2 — custom validators**, arbitrary Python registered in
[validators.py](validators.py) and referenced by name:

```yaml
      - name: extreme-shift
        validator: wav_not_silent
        min_rms_db: -40      # extra case keys parameterize the validator
```

A validator receives `(outputs, controls, case)` — `outputs` maps output
labels to local file paths (file outputs) or decoded objects (JSON
outputs) — and raises `AssertionError` with a helpful message on failure.
Three ship out of the box:

- `wav_not_silent` — asserts WAV outputs carry signal, measured as RMS in
  dBFS (0 dBFS = full scale). The default threshold of `-60` dBFS rejects
  digital silence and near-silence; raise it (e.g. `min_rms_db: -40`) to
  demand typical program levels.
- `wav_format` — checks `channels`, `sample_rate`, and/or `min_duration` on
  WAV outputs — e.g. assert mono output at 44100 Hz and at least 1.5s long.
- `has_labels` — asserts a non-empty pyharp `LabelList` was returned
  (optionally `min_labels`).

To add one:

```python
@validator("midi_not_empty")
def midi_not_empty(outputs, controls, case):
    for label, value in outputs.items():
        if isinstance(value, str) and value.lower().endswith((".mid", ".midi")):
            assert os.path.getsize(value) > 50, f"'{label}' looks empty"
```

### Step 4: run just that model to iterate

```bash
python model_validation/validate_models.py --spaces teamup-tech/pitch_shifter --verbose
```

The console shows each case's pass/fail; `reports/report.json` has per-case
timings and error details. Once green, the daily CI run picks the case up
automatically — no workflow changes needed.

### Reference: full config.yml schema

See the comment block at the top of [config.yml](config.yml) for the
complete schema in one place: `zerogpu_budget_seconds`, `exclude`,
`include_extra`, and per-model `overrides` (`connect_timeout`,
`process_timeout`, `load_only`, `test_cases`).

## Excluding models

Two equivalent ways, merged together:

- `exclude` in [config.yml](config.yml) — for permanent exclusions
  (non-HARP spaces, archived deployments, known-broken examples).
- `--exclude <model> ...` on the command line — for ad-hoc runs.

Use the space id (`teamup-tech/some-space`) for remote models and
`examples/<example-dir>` (e.g. `examples/midi_synthesizer`) for local examples.

## CI behavior

- **Schedule:** daily at 06:30 UTC; also runnable manually from the Actions
  tab (with options to validate specific spaces, skip inference, or disable
  the default restart of crashed spaces).
- **Reports:** markdown summary on each run + JSON/markdown artifacts.
- **Failure signal:** model failures keep the run green (deployments are not
  yet stable enough for daily failure emails to be useful) — they show up as
  warning annotations and in the run summary/report instead. No issues are
  opened and no emails are sent. Only infrastructure errors (bad token,
  discovery failure) turn the run red, since those mean validation itself
  has stopped working.
- **Opting back into notifications later:** remove the exit-code handling in
  the two `Validate` steps of the workflow so the script's exit code 1
  propagates; failed runs then turn red and GitHub emails maintainers.
- Example failures indicate a pyharp/gradio-level breakage that likely
  affects every deployment — fix those before debugging individual spaces.
