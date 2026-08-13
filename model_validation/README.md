# HARP Model Validation

Automated validation of HARP model deployments. Verifies that each deployment
is reachable, exposes the HARP gradio endpoints (`/controls` and `/process`),
and can actually process test inputs end to end. These are the same
interactions the HARP client performs, driven headlessly.

## Overview

Two tiers share the same test harness:

| Tier | Command | What it validates | What a failure means |
|---|---|---|---|
| **Examples** | `--local-examples` | The apps under `pyharp/examples/`, launched locally | pyharp or gradio itself is broken |
| **Spaces** | (default) | Every Hugging Face Space under `teamup-tech` | That specific deployment is broken (build error, HF platform change, dependency drift, ...) |

Key behaviors:

- **Models are validated independently.** A crash, hang, or timeout in one
  never stops the rest. All results are collected into a single report, and
  the process exits non-zero only after everything has run.
- **Crashed and stopped spaces are restarted automatically.** Sleeping spaces
  are woken simply by connecting. Pass `--no-restart-failed` to disable the
  restart, which is also what lets the run work with a read-only token.
- **ZeroGPU time is tracked** across the run and reported after each ZeroGPU
  model. Pass `--skip-zerogpu` to spend no allowance at all.
- A daily GitHub Action
  ([model_validation.yml](../.github/workflows/model_validation.yml)) runs
  both tiers at 06:30 UTC. Model failures do **not** turn the run red, so no
  notification emails arrive while deployments stabilize. Results live in the
  run summary and the report artifacts.

## Code Layout

The modules under `src/` fall into five groups. Dependencies only ever run
downward, so a module can be read knowing nothing about the groups above it:

| Group | File | Purpose |
|---|---|---|
| **Entry point** | [src/validate_models.py](src/validate_models.py) | Command line, per-tier orchestration, console output |
| **Tier drivers** | [src/spaces.py](src/spaces.py) | Gets a Space ready: runtime stage, restart, wake, ZeroGPU safeguards |
| | [src/examples.py](src/examples.py) | Launches a local pyharp app and captures its log |
| **Harness** | [src/harness.py](src/harness.py) | Drives /controls + /process against whatever the drivers hand it |
| **Checking** | [src/cases.py](src/cases.py) | Input synthesis, test-case overlay, output validation and inspection |
| | [src/validators.py](src/validators.py) | Registry of custom output validators (to be extended) |
| **Support** | [src/assets.py](src/assets.py) | Synthesizes the WAV/MIDI/text/JSON test inputs |
| | [src/audio.py](src/audio.py) | Audio decoding (any libsndfile format) for output checks |
| | [src/midi.py](src/midi.py) | MIDI parsing (via mido) for output checks |
| | [src/quota.py](src/quota.py) | ZeroGPU hardware detection and usage tracking |
| | [src/results.py](src/results.py) | Result records and JSON/markdown report generation |
| | [src/utils.py](src/utils.py) | Token handling, error rendering, config, name qualification, timeouts |

Alongside the code, [config.yml](config.yml) holds the validation
configuration and [test_data/](test_data/) holds the real input files that
test cases refer to. Everything a run produces goes under `reports/`.

## Token Setup

ZeroGPU spaces require an authenticated request to obtain GPU quota, and
private spaces require read access. Validation reads the token **only** from
the `HF_TOKEN` environment variable.

- **Never** commit a token, paste it in an issue or PR, or pass it as a
  command-line argument (argv is visible in process listings).
- **CI:** add the token as a repository secret named `HF_TOKEN`
  (Settings → Secrets and variables → Actions). GitHub masks secrets in logs.
- **Locally:** `export HF_TOKEN=...` in your shell (consider `read -s` so it
  stays out of shell history).
- Use a [fine-grained token](https://huggingface.co/settings/tokens) with
  write access to the org's spaces. That access is what the default restart of
  crashed spaces needs. Read access is enough with `--no-restart-failed`.
- **If a token is ever exposed, rotate it immediately** at
  https://huggingface.co/settings/tokens.

## Running Locally

```bash
pip install -r model_validation/requirements.txt

# All spaces in the org (crashed/stopped spaces are restarted by default)
export HF_TOKEN=hf_...   # see token setup above
python model_validation/src/validate_models.py

# Specific spaces, with verbose errors (useful while developing a test case)
python model_validation/src/validate_models.py --spaces pitch_shifter --verbose
python model_validation/src/validate_models.py --spaces pitch_shifter harp-vampnet other-org/model

# Exclude specific models (also configurable via `exclude` in config.yml)
python model_validation/src/validate_models.py --exclude broken-space

# Availability + /controls only (fast, no inference, no GPU quota used)
python model_validation/src/validate_models.py --load-only

# Skip ZeroGPU models, to spend none of the shared ZeroGPU allowance
python model_validation/src/validate_models.py --skip-zerogpu

# Never restart spaces (works with a read-only token)
python model_validation/src/validate_models.py --no-restart-failed

# Examples tier (needs pyharp + example deps, no token required)
pip install torch torchaudio --index-url https://download.pytorch.org/whl/cpu
pip install -e ./pyharp
python model_validation/src/validate_models.py --local-examples
python model_validation/src/validate_models.py --local-examples pitch_shifter
```

**Naming models.** `--spaces` names Hugging Face Spaces and `--local-examples`
names local examples. Neither accepts the other's models. Both expand a bare
name for you, so `pitch_shifter` means `teamup-tech/pitch_shifter` for
`--spaces` and the `pyharp/examples/pitch_shifter` directory for
`--local-examples`. Write `<owner>/<name>` on `--spaces` to reach another
organization, or give `--local-examples` a path to an example kept outside
`pyharp/examples/`.

`--exclude` and the config's `exclude` and `overrides` keys can refer to
either tier. A bare name there means the model in whichever tier is being run,
so one entry covers a model in both. Qualify it to pin it to one tier.
`teamup-tech/<name>` is only ever a space, and `examples/<name>` is only ever
a local example.

## Run Output

Each run writes to its own timestamped directory under
`model_validation/reports/`, so runs never overwrite each other. Override the
base with `--output-dir`. A run directory looks like this:

```
model_validation/reports/2026-07-18T14-30-00/   (local time)
├── report.json      machine-readable results
├── report.md        results table, with full error text below it
├── assets/          the test inputs synthesized for this run
└── pitch_shifter/   one per local example (examples tier only)
    ├── app.log      the example's stdout and stderr
    └── _outputs/    whatever the example itself wrote
```

Both reports record the command line the run was invoked with. Exit code is
`0` when everything passes, `1` on any model failure, and `2` when the run
could not proceed at all (missing token, no models found, bad configuration).

Each example runs in its own directory here rather than in its source
directory. pyharp writes model outputs to an `_outputs` folder under the
working directory, so this keeps a run's files together and leaves the pyharp
checkout untouched.

## ZeroGPU Usage Tracking

Each model's line shows its hardware. ZeroGPU models additionally show the
ZeroGPU work done so far this run:

```
✅ PASS teamup-tech/pitch_shifter (42.1s) [zero-a10g | ZeroGPU: 2 calls, ~84s wall (approx)]
✅ PASS teamup-tech/cpu_model     (18.3s) [cpu-basic]
```

Two figures are reported, because the amount Hugging Face actually bills
cannot be read from the client.

**Call count** is the number of `/process` calls that reached the GPU. It is
exact, and since every such call reserves allowance, it is the most reliable
signal of how much of the allowance a run uses. A call retried after an
infrastructure fault reserves again and counts again. A case that was skipped
for want of an input, or that never left the queue, does not count. CPU and
dedicated-hardware models never contribute.

**Wall time** is how long those calls took, measured from submission to
result. Treat it as a rough ceiling on the GPU time a run costs rather than as
the bill, for two reasons. It includes the queue wait before the job ran,
which is not GPU time. And ZeroGPU charges dynamically: a call reserves its
declared `@spaces.GPU(duration=...)` up front, then refunds the unused part
when the function returns, so the settled amount is usually lower. The
`(approx)` marker is a reminder of both.

Each case's wall time is also recorded in `report.json` as `duration`. All of
these figures describe the work this run does. None of them say anything about
the allowance remaining on the account, for which Hugging Face exposes no
reliable public API. To spend none of the allowance, pass `--skip-zerogpu`.
Those models then appear as `SKIP` in the report.

Three mechanisms limit the ZeroGPU allowance a run can consume:

- **Quota exhaustion stops the remaining ZeroGPU models.** If a ZeroGPU model
  fails with a "quota exceeded" error, every remaining ZeroGPU model is
  skipped (as `SKIP`) rather than run against an exhausted allowance.
- **ZeroGPU models use a lower execution timeout.**
  `--zerogpu-process-timeout` (default 120s) applies instead of
  `--process-timeout` (default 600s). It bounds execution only. The queue wait
  before a job runs is bounded separately by `--connect-timeout`, so a long
  queue does not trip the execution timeout. A per-model `process_timeout`
  override takes precedence over both.
- **ZeroGPU models run one at a time.** `--zerogpu-workers` (default 1) caps
  them, since overlapping reservations tie up allowance that none of them is
  using. Non-ZeroGPU models still run at full `--workers` concurrency
  alongside. The cap is held only around the endpoint tests, so slow restarts
  and connections still overlap.

## Retried Failures

Failures that are not the model's fault are retried rather than reported.

A "model is still loading" response comes from a space starting its GPU
worker. It is retried until `--connect-timeout`, the time budgeted for a model
to come up.

A short list of infrastructure faults is retried a few times at a short
interval instead. It currently covers a read timeout, a GPU host ECC error,
and a server disconnect. These have no expected duration to wait out, and the
retry usually surfaces the model's real behaviour.

That list is `TRANSIENT_MARKERS` in [src/harness.py](src/harness.py), retried
`TRANSIENT_RETRY_LIMIT` times. It is deliberately narrow and holds only faults
observed to clear on a retry. A retried `/process` call reserves ZeroGPU
allowance again, so a marker that also fires on a genuine failure multiplies
the quota that failure costs and delays the real error. That rules out
matching a dropped connection broadly, because a model that crashes its own
Space drops the connection in exactly the same way. Add an entry only once a
fault is confirmed to clear on a retry.

## Configuring Test Cases

Every model automatically gets one **`default`** test case, even with no
configuration at all. Its inputs are synthesized from the model's `/controls`
spec. Audio tracks get a sine-sweep clip, MIDI tracks get a short MIDI file,
and sliders, toggles, dropdowns, number boxes, and text boxes get the default
value each one declares. A control that declares no default falls back to
something valid for its type, such as its minimum or its first dropdown
choice, so every model can be exercised without configuration.

Configuration is only needed to go beyond that: pinning specific control
values, feeding real audio, adjusting the properties of the synthesized
inputs, or inspecting outputs more deeply.

### Synthesized input properties

The generated inputs default to a 2-second mono 44.1 kHz WAV and a two-note
MIDI file. Override any of those properties with a `synthesized_inputs` block.
It can be set globally in [config.yml](config.yml), per model under its
`overrides` entry, or per test case. Each level overrides the one before it:

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

Audio is written with soundfile, so `ext` accepts any format this libsndfile
build can write. An extension it cannot write is reported as a configuration
error before any model runs, rather than quietly leaving models without
inputs. The extension is a preference rather than a guarantee. A component
that only accepts other extensions still gets one it accepts. Each distinct
set of properties is generated once and reused for the rest of the run.

### Step 1: find the model's control labels

Test cases reference inputs and outputs by their **label**, the display name
each gradio component was given in the model's `app.py`. There are three ways
to find them:

- Open the Space's gradio UI and read the component titles, or click its
  "View Controls" button to see the full spec as JSON.
- Read the model's `app.py`, where each component has a `label=...`.
- Reference a wrong label on purpose and run the validator. The error message
  lists every available label.

### Step 2: add the case to config.yml

Cases live under `overrides:`, keyed by space id (or by `examples/<example-dir>`
for local examples). Each case has a `name` plus any of three optional fields.
`controls` overrides scalar values by label. `files` substitutes input files by
label, with paths taken relative to config.yml, so real inputs belong in
`test_data/`. `process_timeout` extends the timeout for that case alone.

```yaml
overrides:
  teamup-tech/pitch_shifter:
    process_timeout: 900          # this model is slow, extend the timeout
    test_cases:
      - name: default             # keep the synthesized case too
      - name: extreme-shift
        controls:
          "Pitch Shift (semitones)": 24
      - name: real-audio
        files:
          "Input Audio A": test_data/short_vocal.wav
```

Note: once `test_cases` is present, **only** the listed cases run. Include
`- name: default` to keep the synthesized one.

### Step 3: check the outputs (optional)

Every case already gets structural checks for free. `/process` must not error,
file outputs must exist and be non-empty, and a JSON output (an optional
pyharp `LabelList`) must be well-formed when present. An absent or `None`
label output is valid, since labels are optional.

Beyond that, there are two mechanisms, divided by what they can express:

- **`expect`** asserts properties of a *single* output, and covers most
  checks.
- **`validators`** run custom Python for checks that cannot be expressed as a
  property of one output, typically relationships spanning several outputs.

#### `expect`: declarative per-output rules

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
| `bit_depth` | audio output | Exact PCM bit depth (16, 24, ...), and errors on compressed formats such as MP3 or OGG |
| `min_rms_db` | audio output | RMS level is at least this many dBFS |
| `min_duration` / `max_duration` | audio or MIDI output | Length in seconds is within bounds |
| `min_notes` | MIDI output | At least this many note-on events |
| `min_labels` | JSON (`LabelList`) output | At least this many labels returned |

**Targeting outputs.** A model with several outputs gets one block per output
label, each checked independently. Use `"*"` instead of a label to apply rules
to every output a rule covers. With mixed outputs, `"*"` sends `min_bytes` to
all file outputs, `min_rms_db` only to the audio ones, and `min_notes` only to
the MIDI ones:

```yaml
        expect:
          "*":
            min_bytes: 1000      # every file output must be non-trivial
```

**Mistakes on a named output are surfaced as configuration mistakes, not as
model faults.** An unrecognized rule name, an unknown output label, or a rule
aimed at a named output whose type it does not cover (`min_labels` on an audio
output, say) fails the case with a message naming the problem and listing what
is valid. The run still exits `1`, since the case did not pass, but it is the
config that needs the fix. A rule under `"*"` is more forgiving. When it
matches no compatible output it is skipped instead of raising an error. That
is what lets a generic case (see
[common test cases](#step-4-reuse-a-case-across-models)) target output types a
given model does not have.

**On `min_rms_db`:** level is measured as RMS in dBFS, where 0 dBFS is full
scale. It is a bit-depth-independent unit that is easy to set thresholds in.
Digital silence is `-inf`, quiet noise floors sit near `-60`, and typical
program material is above `-40`. RMS is also more robust than a peak
measurement, since a single stray click cannot make an otherwise-silent file
pass. `min_rms_db: -60` is the usual "this output is not silent" check.

**On `min_labels`:** `min_labels: 0` is meaningful, and is the right rule when
a model may legitimately return no labels. It asserts that a well-formed
`LabelList` came back while permitting it to be empty. Omitting the rule
entirely is weaker, because a missing or `None` label output also passes.
Labels are optional in pyharp.

**On audio formats:** audio outputs are decoded with
[soundfile](https://python-soundfile.readthedocs.io/), a listed dependency, so
audio rules work on any libsndfile format. That covers WAV, FLAC, OGG, MP3,
AIFF, and more, not just pyharp's `save_audio()` WAV default. Decoding to
float also makes every audio rule bit-depth-independent, so a level threshold
means the same thing whether the source is 16-bit, 24-bit, or float.
`bit_depth` is the exception, since it reads the file's PCM encoding and
errors on compressed formats, which have no fixed bit depth.

**On MIDI:** MIDI outputs are parsed with
[mido](https://mido.readthedocs.io/), also a listed dependency.
`min_duration` and `max_duration` are shared with audio and read the MIDI's
own length in seconds. `min_notes` counts note-on events.

#### `validators`: custom Python

Registered in [validators.py](src/validators.py) and referenced by name, with
each validator's parameters nested beneath it so they stay separate from the
test case's own fields. A case may run several:

```yaml
      - name: labels-line-up
        validators:
          labels_within_audio:
            tolerance: 0.1
```

A validator is a function taking three arguments:

| Argument | What it holds |
|---|---|
| `outputs` | Each output label mapped to its value. File outputs are local filesystem paths, and JSON outputs are the decoded object. |
| `controls` | The full `/controls` payload (card, inputs, outputs). |
| `params` | The settings nested under this validator's name in the test case. |

To report a problem, a validator raises `AssertionError`. The message should
name the output at fault and say what was wrong with it. To opt out on a model
that lacks the outputs it needs, a validator raises `ValidatorNotApplicable`
instead. That counts as a skip rather than a failure, which is what lets a
validator be used in a common test case.

The shipped `labels_within_audio` does both. It asserts that every returned
label falls inside the audio output's timespan, a relationship between two
outputs that no single-output rule can express, and it opts out when the model
has no audio output or no label output.

To add a validator:

```python
@validator("labels_sorted")
def labels_sorted(outputs, controls, params):
    """Assert every LabelList output is in chronological order, an ordering
    property no single-value `expect` rule can express."""
    for label, value in outputs.items():
        if isinstance(value, dict) and isinstance(value.get("labels"), list):
            times = [entry.get("t", 0.0) for entry in value["labels"]]
            assert times == sorted(times), f"'{label}' labels are out of order"
```

Before writing one, check whether an `expect` rule would do. If the check is a
generally useful property of a single output, consider adding it to the
`expect` vocabulary (`EXPECT_RULES` in [cases.py](src/cases.py)) instead of
putting it in a one-off validator.

### Step 4: reuse a case across models

A case under a model's `test_cases` only applies to that model. For a check
that should hold for *every* model, such as "no file output is empty" or
"audio is never silent", add a `common_test_cases` entry at the top level of
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

Keep common cases model-agnostic by targeting outputs with `"*"` rather than a
specific label, which will not exist on most models. Because a `"*"` rule is
skipped when it matches no compatible output, `audio-not-silent` above checks
audio models and is skipped on MIDI-only ones. A validator can do the same by
raising `ValidatorNotApplicable` when the model lacks the outputs it needs, as
the shipped `labels_within_audio` does. A model can opt out of common cases
entirely with `skip_common_cases: true` in its `overrides` entry.

### Step 5: run just that model to iterate

```bash
python model_validation/src/validate_models.py --spaces teamup-tech/pitch_shifter --verbose
```

The console shows each case's pass or fail, and `report.json` has the per-case
timings and error details. Once the case passes reliably, the daily CI run
picks it up automatically. No workflow changes are needed.

### Reference: full config.yml schema

See the comment block at the top of [config.yml](config.yml) for the complete
schema in one place. It covers `exclude`, `include_extra`,
`synthesized_inputs`, top-level `common_test_cases`, and per-model `overrides`
(`connect_timeout`, `process_timeout`, `load_only`, `test_cases`,
`skip_common_cases`).

## Excluding Models

There are two equivalent ways, and they are merged together:

- `exclude` in [config.yml](config.yml), for permanent exclusions such as
  non-HARP spaces, archived deployments, and known-broken examples.
- `--exclude <model> ...` on the command line, for ad-hoc runs.

Models are named as described above. `midi_synthesizer` excludes the space on
a spaces run and the example on an examples run, while
`examples/midi_synthesizer` excludes only the example.

## CI Behavior

- **Schedule:** daily at 06:30 UTC. It can also be run manually from the
  Actions tab, with options to validate specific spaces, skip inference, or
  disable the default restart of crashed spaces.
- **Reports:** a markdown summary on each run, plus JSON and markdown
  artifacts.
- **Failure signal:** model failures keep the run green, because deployments
  are not yet stable enough for daily failure emails to be useful. They show
  up as warning annotations and in the run summary and report instead. No
  issues are opened and no emails are sent. Only infrastructure errors, such
  as a bad token or a discovery failure, turn the run red, since those mean
  validation itself has stopped working.
- **Opting back into notifications later:** remove the exit-code handling in
  the two `Validate` steps of the workflow so the script's exit code 1
  propagates. Failed runs then turn red and GitHub emails maintainers.
- A failure in the examples tier likely indicates a pyharp or gradio level
  breakage that may affect every deployment. Fix those before debugging
  individual spaces.
