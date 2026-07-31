# HARP Model Validation

Automated validation of HARP model deployments. Verifies that each deployment
is reachable, exposes the HARP gradio endpoints (_i.e._, `/controls`, `/process`),
and can actually process test inputs end-to-end — the same interactions the
HARP client performs, driven headlessly.

## Overview

Two tiers share the same test harness:

| Tier | Command | What it validates | What a failure means |
|---|---|---|---|
| **Examples** | `--local-examples` | The apps under `pyharp/examples/`, launched locally | pyharp or gradio itself is broken |
| **Spaces** | (default) | Every Hugging Face Space under `teamup-tech` | That specific deployment is broken (build error, HF platform change, dependency drift, ...) |

Key behaviors:

- **Models are validated independently** — a crash, hang, or timeout in one
  never stops the rest; all results are collected into a single report and
  the process exits non-zero only after everything has run.
- **Crashed/stopped spaces are restarted automatically** (sleeping spaces
  are woken simply by connecting). Pass `--no-restart-failed` to disable;
  restarting requires a token with write access.
- **ZeroGPU time is tracked** across the run and reported after each ZeroGPU
  model; `--skip-zerogpu` skips those models to spend no allowance at all.
- A daily GitHub Action
  ([model_validation.yml](../.github/workflows/model_validation.yml)) runs
  both tiers at 06:30 UTC. Model failures do **not** turn the run red (no
  notification emails while deployments stabilize) — results live in the
  run summary and report artifacts.

## Code layout

 File | Purpose |
|---|---|
| [src/validate_models.py](src/validate_models.py) | Command-line entry point and per-tier orchestration |
| [src/harness.py](src/harness.py) | The test harness: drives /controls + /process against a live model |
| [src/cases.py](src/cases.py) | Synthesis of inputs, test-case overlay, output validation/inspection |
| [src/validators.py](src/validators.py) | Registry of custom output validators (to be extended) |
| [src/audio.py](src/audio.py) | Audio decoding (any libsndfile format) for output checks |
| [src/midi.py](src/midi.py) | MIDI parsing (via mido) for output checks |
| [src/assets.py](src/assets.py) | Synthesized WAV/MIDI/text/JSON test inputs |
| [src/quota.py](src/quota.py) | ZeroGPU usage tracking and account quota lookup |
| [src/results.py](src/results.py) | Result records and JSON/markdown report generation |
| [src/utils.py](src/utils.py) | Token handling, config loading, discovery, timeouts |
| [config.yml](config.yml) | Validation configuration (_e.g._, excludes, per-model test cases) |
| [test_data/](test_data/) | Real input files referenced by test cases |
| `reports/` | Generated reports, synthesized assets, and example logs |

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
python model_validation/src/validate_models.py

# A single space, with verbose errors (useful while developing a test case)
python model_validation/src/validate_models.py --spaces teamup-tech/pitch_shifter --verbose

# Exclude specific models (also configurable via `exclude` in config.yml)
python model_validation/src/validate_models.py --exclude teamup-tech/broken-space

# Availability + /controls only (fast; no inference, no GPU quota used)
python model_validation/src/validate_models.py --load-only

# Skip ZeroGPU models, to spend none of the shared ZeroGPU allowance
python model_validation/src/validate_models.py --skip-zerogpu

# Never restart spaces (works with a read-only token)
python model_validation/src/validate_models.py --no-restart-failed

# Examples tier (needs pyharp + example deps; no token required)
pip install torch torchaudio --index-url https://download.pytorch.org/whl/cpu
pip install -e ./pyharp
python model_validation/src/validate_models.py --local-examples
```

Each run writes to its own timestamped directory under
`model_validation/reports/` (override the base with `--output-dir`), so runs
never overwrite each other — e.g. `model_validation/reports/2026-07-18T14-30-00Z/`.
It contains `report.json` (machine-readable) and `report.md` (human-readable
table), with synthesized test inputs and local example logs alongside. Both
reports record the command line the run was invoked with. Exit code is `0`
when everything passes, `1` on any model failure, `2` on
configuration/infrastructure errors.

## ZeroGPU usage tracking

Each model's line shows its hardware, and ZeroGPU models additionally show
the ZeroGPU work done so far this run:

```
✅ PASS teamup-tech/pitch_shifter (42.1s) [zero-a10g | ZeroGPU: 2 calls, ~84s wall (approx)]
✅ PASS teamup-tech/cpu_model     (18.3s) [cpu-basic]
```

Two figures are tracked, because the exact billed amount is not observable
from the client:

- **Call count** — the number of `/process` calls that reached the GPU. This
  is exact and is the most reliable signal of how much of the allowance a run
  will use. Only calls that ran count; a queued or input-skipped case does
  not. CPU and dedicated-hardware models never contribute.
- **Wall time** — the total `/process` wall time (queue plus execution) of
  those calls, marked `(approx)`. Hugging Face bills ZeroGPU **dynamically**:
  each call reserves its declared `@spaces.GPU(duration=)` time up front and
  refunds the unused part when the function returns, so an account's usage
  rises during a run and settles lower afterwards. This wall figure is an
  over-estimate of the settled bill — closer to that mid-run reservation peak
  — and is not returned to the client, so treat it as indicative. Set
  `zerogpu_budget_seconds` in [config.yml](config.yml) (_e.g._, `1500` for the
  PRO 25 min/day allowance) to show it against a budget.

Each case's wall time is also recorded in `report.json` (`duration`). These
figures reflect the work *this run* does, not your account's remaining quota —
Hugging Face exposes no reliable public API for the latter. To spend none of
the allowance, pass `--skip-zerogpu`, which skips ZeroGPU models entirely
(they appear as `SKIP` in the report).

Two mechanisms limit the ZeroGPU allowance a run can consume:

- **Quota exhaustion stops the remaining ZeroGPU models.** If a ZeroGPU model
  fails with a "quota exceeded" error, every remaining ZeroGPU model is
  skipped (as `SKIP`) rather than run against an exhausted allowance.
- **ZeroGPU models use a lower execution timeout** — `--zerogpu-process-timeout`
  (default 120s) instead of `--process-timeout` (default 600s). This bounds
  execution only; the queue wait before a job runs is bounded separately by
  `--connect-timeout`, so a long queue does not trip the execution timeout. A
  per-model `process_timeout` override takes precedence.
- **ZeroGPU models run one at a time** — `--zerogpu-workers` (default 1). Each
  concurrent GPU call reserves its declared duration, so overlapping them ties
  up allowance that is not being used. Non-ZeroGPU models still run at full
  `--workers` concurrency alongside, and the limit is held only around the
  endpoint tests, so slow restarts and connections still overlap.

Transient "model is still loading" responses from a ZeroGPU space waking its
GPU worker are retried automatically (up to `connect_timeout`), rather than
counted as failures.

## Configuring test cases — a walkthrough

Every model automatically gets one **`default`** test case, even with no
configuration at all: inputs are synthesized from the model's `/controls`
spec — a sine-sweep clip for audio tracks, a short MIDI file for MIDI tracks,
and each control's declared default value for sliders, toggles, dropdowns,
number boxes, and text boxes. So configuration is only needed to go beyond
that: pinning specific control values, feeding real audio, adjusting the
properties of the synthesized inputs, or inspecting outputs more deeply.

### Synthesized input properties

The generated inputs default to a 2-second mono 44.1 kHz WAV and a two-note
MIDI file. Override any of those properties with a `synthesized_inputs` block
— globally in [config.yml](config.yml), per model under its `overrides` entry,
or per test case; each level overrides the last:

```yaml
synthesized_inputs:      # global default for every model
  audio:
    sample_rate: 48000
    channels: 2
    duration: 5.0
    ext: .flac
  midi:
    num_notes: 8
    note_duration: 0.25
```

Audio is written with soundfile, so `ext` accepts any libsndfile format. It is
a preference rather than a guarantee: a component that only accepts other
extensions still gets one it accepts. Each distinct set of properties is
generated once and reused for the rest of the run.

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

### Step 3: check the outputs (optional)

Every case already gets structural checks for free: `/process` must not
error, file outputs must exist and be non-empty, and JSON outputs (_i.e._, an
optional pyharp `LabelList`) must be well-formed when present (absent/None
is valid, since labels are optional).

Beyond that, there are two mechanisms, divided by what they can express:

- **`expect`** asserts properties of a *single* output, and covers most
  checks.
- **`validators`** run custom Python for checks that cannot be expressed as
  a property of one output — typically relationships spanning several
  outputs.

#### `expect` — declarative per-output rules

Keyed by output label, then by rule. Every rule is optional, and an output
with no rules is still subject to the structural checks above:

```yaml
      - name: extreme-shift
        controls:
          "Pitch Shift (semitones)": 24
        expect:
          "Output Audio":
            ext: .wav            # extension: a string, or a list of accepted ones
            min_bytes: 10000     # minimum file size
            channels: 1          # exact channel count (1 = mono, 2 = stereo)
            sample_rate: 44100   # exact sample rate, in Hz
            min_duration: 1.5    # minimum length, in seconds
            max_duration: 10.0   # maximum length, in seconds
            bit_depth: 16        # exact PCM bit depth
            min_rms_db: -60      # minimum RMS level, in dBFS
          "Output Labels":
            min_labels: 1        # minimum labels in a pyharp LabelList
```

The full vocabulary, and which output types each rule covers:

| Rule | Applies to | Asserts |
|---|---|---|
| `ext` | any file output | Extension matches (string, or list of accepted extensions) |
| `min_bytes` | any file output | File is at least this many bytes |
| `channels` | audio output | Exact channel count |
| `sample_rate` | audio output | Exact sample rate in Hz |
| `bit_depth` | audio output | Exact PCM bit depth (16, 24, ...); errors on compressed formats (_e.g._, MP3 or OGG) |
| `min_rms_db` | audio output | RMS level is at least this many dBFS |
| `min_duration` / `max_duration` | audio or MIDI output | Length in seconds is within bounds |
| `min_notes` | MIDI output | At least this many note-on events |
| `min_labels` | JSON (`LabelList`) output | At least this many labels returned |

**Targeting outputs.** A model with several outputs gets one block per
output label, each checked independently. Use `"*"` instead of a label to
apply rules to every output a rule covers — with mixed outputs, `"*"` sends
`min_bytes` to all file outputs, `min_rms_db` only to the audio ones, and
`min_notes` only to the MIDI ones:

```yaml
        expect:
          "*":
            min_bytes: 1000      # every file output must be non-trivial
```

**Mistakes on a named output are configuration errors, not model failures.**
An unrecognized rule name, an unknown output label, or a rule aimed at a named
output whose type it does not cover (_e.g._, `min_labels` on an audio output)
raises an error naming the problem and listing what is valid. A rule under
`"*"` is more forgiving: when it matches no compatible output, it is skipped
instead of raising an error. This lets a generic case (see
[common test cases](#step-4-reuse-a-case-across-models)) target output types
that a given model does not have.

**On `min_rms_db`:** level is measured as RMS in dBFS (0 dBFS = full scale),
a bit-depth-independent unit that is easy to set thresholds in. Digital
silence is `-inf`, quiet noise floors sit near `-60`, and typical program
material is above `-40`. RMS is also more robust than a peak measurement — a
single stray click cannot make an otherwise-silent file pass. `min_rms_db:
-60` is the usual "this output is not silent" check.

**On `min_labels`:** `min_labels: 0` is meaningful and is the right rule when
a model may legitimately return no labels — it asserts a well-formed
`LabelList` came back while permitting it to be empty. Omitting the rule
entirely is weaker: a missing or `None` label output also passes, because
labels are optional in pyharp.

**On audio formats:** audio outputs are decoded with
[soundfile](https://python-soundfile.readthedocs.io/) (a listed dependency),
so audio rules work on any libsndfile format — WAV, FLAC, OGG, MP3, AIFF, and
more — not just pyharp's `save_audio()` WAV default. Decoding to float also
makes every audio rule bit-depth-independent, so a level threshold means the
same thing whether the source is 16-bit, 24-bit, or float. `bit_depth` is the
exception: it reads the file's PCM encoding and errors on compressed formats,
which have no fixed bit depth.

**On MIDI:** MIDI outputs are parsed with
[mido](https://mido.readthedocs.io/) (a listed dependency). `min_duration` /
`max_duration` are shared with audio and read the MIDI's own length in
seconds; `min_notes` counts note-on events.

#### `validators` — custom Python

Registered in [validators.py](src/validators.py) and referenced by name, with
each validator's parameters nested beneath it, so they stay separate from the
test case's own fields. A case may run several:

```yaml
      - name: labels-line-up
        validators:
          labels_within_audio:
            tolerance: 0.1
```

A validator receives `(outputs, controls, params)` — `outputs` maps output
labels to local file paths (file outputs) or decoded objects (JSON outputs) —
and raises `AssertionError` with a message naming the output and what went
wrong. It may raise `ValidatorNotApplicable` to opt out on a model that lacks
the outputs it needs; that is treated as a skip, not a failure, so a validator
can be used in a common test case. As an example, `labels_within_audio`
asserts every returned label falls inside the audio output's timespan (a
relationship between two outputs), and raises `ValidatorNotApplicable` when
the model has no audio or no label output.

To add a validator:

```python
@validator("labels_sorted")
def labels_sorted(outputs, controls, params):
    """Assert every LabelList output is in chronological order - an ordering
    property no single-value `expect` rule can express."""
    for label, value in outputs.items():
        if isinstance(value, dict) and isinstance(value.get("labels"), list):
            times = [entry.get("t", 0.0) for entry in value["labels"]]
            assert times == sorted(times), f"'{label}' labels are out of order"
```

Before writing one, check whether an `expect` rule would do — and if the
check is a generally useful property of a single output, consider adding it
to the `expect` vocabulary (`EXPECT_RULES` in [cases.py](src/cases.py))
instead of putting it in a one-off validator.

### Step 4: reuse a case across models

A case under a model's `test_cases` only applies to that model. For a check
that should hold for *every* model — "no file output is empty", "audio is
never silent" — add a `common_test_cases` entry at the top level of
config.yml instead. Common cases run on every model in addition to its own,
and their names are shown in the report prefixed with `common:`.

```yaml
common_test_cases:
  - name: outputs-nontrivial
    expect:
      "*":
        min_bytes: 100      # every file output, on every model
  - name: audio-not-silent
    expect:
      "*":
        min_rms_db: -60     # applies only to models that have audio outputs
```

Keep common cases model-agnostic: target outputs with `"*"` (never a specific
label, which will not exist on most models). Because a `"*"` rule is skipped
when it matches no compatible output, `audio-not-silent` above checks audio
models and is skipped on MIDI-only ones. A validator can do the same by
raising `ValidatorNotApplicable` when the model lacks the outputs it needs
(the shipped `labels_within_audio` does this). A model can opt out of common
cases entirely with `skip_common_cases: true` in its `overrides` entry.

### Step 5: run just that model to iterate

```bash
python model_validation/src/validate_models.py --spaces teamup-tech/pitch_shifter --verbose
```

The console shows each case's pass/fail; `report.json` has per-case timings
and error details. Once the case passes reliably, the daily CI run picks it
up automatically — no workflow changes needed.

### Reference: full config.yml schema

See the comment block at the top of [config.yml](config.yml) for the
complete schema in one place: `zerogpu_budget_seconds`, `exclude`,
`include_extra`, top-level `common_test_cases`, and per-model `overrides`
(`connect_timeout`, `process_timeout`, `load_only`, `test_cases`,
`skip_common_cases`).

## Excluding models

Two equivalent ways, merged together:

- `exclude` in [config.yml](config.yml) — for permanent exclusions
  (non-HARP spaces, archived deployments, known-broken examples).
- `--exclude <model> ...` on the command line — for ad-hoc runs.

Use the space id (`teamup-tech/<some-space>`) for remote models and
`examples/<some-example>` for local examples.

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
- Failures with example models likely indicate a pyharp/gradio-level breakage that may
  affect every deployment — fix those before debugging individual spaces.
