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
| **Spaces** | (default) | Every Hugging Face Space under `--org` | That specific deployment is broken (build error, HF platform change, dependency drift, ...) |

Key behaviors:

- **Models are validated independently.** A crash, hang, or timeout in one
  never stops the rest. All results are collected into a single report, and
  the process exits non-zero only after everything has run.
- **Crashed and stopped spaces are restarted automatically**, and a sleeping
  space is woken by a request that waits out its start. Pass
  `--no-restart-failed` to disable the restart. This is also what allows a
  run to work with a read-only token.
- **ZeroGPU time is tracked** across the run and reported after each ZeroGPU
  model. Pass `--skip-zerogpu` to skip these models and avoid spending allowance.
- A daily GitHub Action
  ([model_validation.yml](../.github/workflows/model_validation.yml)) runs
  both tiers. See [CI Behavior](#ci-behavior) for what it reports and for when
  a run turns red.

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
| | [src/expectations.py](src/expectations.py) | The declarative `expect` rule vocabulary and its checks |
| | [src/validators.py](src/validators.py) | Registry of custom output validators (to be extended) |
| **Support** | [src/assets.py](src/assets.py) | Synthesizes the WAV/MIDI/text/JSON test inputs |
| | [src/audio.py](src/audio.py) | Audio decoding (any libsndfile format) for output checks |
| | [src/midi.py](src/midi.py) | MIDI parsing (via mido) for output checks |
| | [src/quota.py](src/quota.py) | ZeroGPU hardware detection and usage tracking |
| | [src/results.py](src/results.py) | Result records and JSON/markdown report generation |
| | [src/utils.py](src/utils.py) | Token handling, error rendering, config, name qualification, timeouts |

Alongside the code, [config.yml](config.yml) holds the validation
configuration and [test_data/](test_data/) holds any real input files the
test cases reference. Everything a run produces goes under `reports/`.

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

A spaces run checks the token once before it starts. A rejected token stops
the run with exit code `2`, rather than failing every space in turn. A token
that is valid but cannot restart spaces in the organization draws a warning,
since only the restart of a crashed space needs write access. The run then
continues, reporting crashed spaces instead of attempting to recover them.

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

# Don't attempt to restart spaces (works with a read-only token)
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

## Selecting Models

A spaces run validates every space under `--org`. Two keys in
[config.yml](config.yml) adjust that standing set in either direction.

`include_extra` adds spaces from outside the organization, _i.e._ a HARP
deployment kept under someone else's account. It describes discovery, so it
does nothing when `--spaces` names what to run, and nothing on an examples run.

`exclude` removes models, _e.g._ non-HARP spaces, archived deployments, and
known-broken examples. It applies to both tiers, and `--exclude <model> ...`
adds to it for one run.

The two exclusion sources differ in one way. A config exclusion is a standing
decision about a model, so it applies to whatever the organization-wide discovery
turns up, but naming that model on the command line overrides it. That is what
you want when checking whether a space you disabled is working again, since
`--spaces some-excluded-space` validates it without your having to edit the
file first. An `--exclude` given on that same command line is just as
specific, so it still applies.

**Naming.** `exclude`, `--exclude`, and the `overrides` keys can refer to
either tier, so qualify them. `teamup-tech/<name>` is only ever a space, and
`examples/<name>` is only ever a local example. A bare name is still accepted
and refers to the model in whichever tier is being run, but qualifying keeps a
spaces entry from silently applying to a like-named example and vice-versa.
Qualify `include_extra` entries too, since a bare name there resolves
to the organization and names a space discovery already covers.

## Run Output

Each run writes to its own timestamped directory under `model_validation/reports/`.
Override the base with `--output-dir`. A run directory looks like this:

```
model_validation/reports/2026-07-18T14-30-00/   (local time)
├── report.json          machine-readable results
├── report.md            results table, with full error text below it
├── assets/              the test inputs synthesized for this run
└── examples/            examples tier only, one directory per example
    └── pitch_shifter/
        ├── app.log      the example's stdout and stderr
        └── _outputs/    whatever the example itself wrote
```

Both reports record the command line the run was invoked with. Exit code is
`0` when everything passes, `1` on any model failure, and `2` when the run
could not proceed at all (missing token, no models found, bad configuration).

Each example runs in its own directory under `examples/` rather than in its
source directory. pyharp writes model outputs to an `_outputs` folder under the
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
the bill, for two reasons:
1. It includes the queue wait before the job ran, which is not GPU time.
2. ZeroGPU charges dynamically. A call reserves its
declared `@spaces.GPU(duration=...)` up front, then refunds the unused part
when the function returns, so the settled amount is usually lower.

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
  queue does not trip the execution timeout. A `process_timeout` set for the
  model in config.yml takes precedence over both.
- **ZeroGPU models run one at a time by default.** `--zerogpu-workers` caps
  them, since overlapping reservations tie up allowance that none of them is
  using. Non-ZeroGPU models still run at full `--workers` concurrency
  alongside. The cap is held only around the endpoint tests, so slow restarts
  and connections still overlap.

## When a Space Will Not Start

A sleeping space wakes automatically when something requests it.
However, sometimes this does not work and the host answers at once with a `503`
and an empty body, and the stage stays `SLEEPING` for however long the request is
held open or for however often it is repeated. Authenticating the request makes no
difference, and neither does loading the space's page.

In these situations a restart will work. The same space enters `BUILDING`, then
`APP_STARTING`, then `RUNNING`. A space in the stuck `SLEEPING`
state appears to have nothing left to resume from, so the router has no
running app to hand the request to and no image to start, while an explicit
restart rebuilds it.

So the harness restarts a sleeping space that refuses to start, provided
restarts are enabled, which needs a token with write access. With a read-only
token the refusal is reported instead, naming the restart failure. The
condition also clears by itself given a few hours, so a space that reports
this one day often validates normally the next.

## Retried Failures

Failures that are not the model's fault are retried rather than reported.

A "model is still loading" response comes from a space starting its GPU
worker. It is retried until `--connect-timeout`, the time budgeted for a model
to come up.

An infrastructure fault is retried `TRANSIENT_RETRY_LIMIT` times at a short
interval instead. Currently covered are a read timeout, a GPU host ECC error,
and a server disconnect. None of these has an expected duration to wait out,
which is why they are bounded by a retry count rather than by a deadline, and
the retry usually surfaces the model's real behaviour.

This list is `TRANSIENT_MARKERS` in [src/harness.py](src/harness.py), and it
is deliberately narrow. A retried `/process` call reserves ZeroGPU allowance
again, so a marker that also fires on a genuine failure multiplies what that
failure costs and delays the real error. It is why a dropped connection is not
matched broadly, since a model that crashes its own Space can drop the connection
in exactly the same way. Add an entry only once a fault is confirmed to clear
on a retry.

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
MIDI file. A `synthesized_inputs` block departs from those defaults. It can be
set at the top level of [config.yml](config.yml), per model under its
`overrides` entry, or per test case, with each level overriding the one before
it. Every property is optional, so a block sets only what it changes.

```yaml
# config.yml, at the top level, so it applies to every model
synthesized_inputs:
  audio:
    sample_rate: 48000   # default 44100
    num_channels: 2      # default 1
    duration: 5.0        # default 2.0
    ext: .flac           # default .wav
  midi:
    num_notes: 8         # default 2
    note_duration: 0.25  # default 0.5
    instrument: 24       # default 0
    channel: 0           # default 0
```

The MIDI `instrument` is the General MIDI program number, written as a program
change, and it is where both HARP and a model read a note's instrument from.
A model that processes only one instrument family needs it set to a program it
accepts, since the default of 0 is a grand piano.

Channels are counted from 0 here, matching the MIDI wire format, so they run 0
to 15 wherever validation reads or reports one. Most DAWs count the same
channels from 1, which is why the General MIDI drum channel is 9 here and is
the channel commonly called "channel 10". Both `instrument` and `channel` are
checked as configuration, so a value outside its range is reported before any
model runs rather than being written into a file the model cannot parse.

Audio and MIDI are the only media a block can configure, because they are the
only inputs built from properties. A generic file input (`gr.File`) accepting
`.txt` or `.json` is handed a fixed placeholder file, which has nothing worth
varying, and one accepting some other format has to be supplied by a test
case's `files` entry. An unrecognized type or property name is reported as a
configuration error before any model runs, rather than being read by nothing
at all.

Scalar controls have nothing to do with this block. A text box, number box,
slider, toggle, or dropdown takes the default value its `/controls` spec
declares, and a test case's `controls:` entry is what overrides it. (A JSON
component is an output type in HARP, so it never needs an input at all.)

Audio is written with soundfile, so `ext` accepts any format this libsndfile
build can write, and one it cannot write is a configuration error.

`ext` is set per media kind (_i.e._ audio or midi), and applies wherever a file of
that kind is synthesized. That includes generic file inputs, because a
`gr.File` accepting `.wav` or `.flac` is a generic file input in HARP terms
rather than an audio track, and what it gets handed is still a synthesized
audio clip. Only `gr.Audio` is an audio track, and only `.mid` or `.midi`
file types make a MIDI track.

An audio or MIDI track gets the configured extension as given. For a generic
file input it is a preference instead, since the component's declared
`file_types` are a contract and one `ext` setting spans every model. A
component accepting only other formats is given one it accepts, and one whose
accepted types cannot be synthesized at all, such as a bespoke binary format,
has its case skipped with a message naming the input. When a specific input
needs a specific file, name it in a test case's `files` entry.

A file for each distinct set of properties is generated once and reused for
the rest of the run.

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

Cases live under `overrides:`, keyed by the model's qualified name, which is
its space id or `examples/<example-dir>`. Each case needs a `name` and takes
several optional fields.

`controls` overrides scalar values by label, and
`files` substitutes input files by label, with paths taken relative to
config.yml (real inputs belong under `test_data/`).
`process_timeout` can be overridden at either the model or case level.
See above for information about `synthesized_inputs` and Step 3 for information about `expect` and `validators`.

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
`- name: default` to keep the default synthesized case.

### Step 3: check the outputs (optional)

Every case already gets structural checks for free. `/process` must not error,
file outputs must exist and be non-empty, and a JSON output (_e.g._ an optional
pyharp `LabelList`) must be well-formed when present. Note that an absent or `None`
label output is valid, since labels are optional.

Beyond that, there are two mechanisms to validate output:

- **`expect`** asserts properties of a *single* output, and covers most
  checks.
- **`validators`** run custom Python for checks that cannot be expressed as a
  property of one output, typically relationships spanning several outputs.

#### `expect`: declarative per-output rules

Keyed by output label, then by rule. Every rule is optional, and an output
with no rules is still subject to the default structural checks:

```yaml
      - name: extreme-shift
        controls:
          "Pitch Shift (semitones)": 24
        expect:
          "Output Audio":
            ext: .wav            # extension: a string, or a list of accepted ones
            min_bytes: 10000     # minimum file size
            num_channels: 1      # exact channel count (1 = mono, 2 = stereo)
            sample_rate: 44100   # exact sample rate, in Hz
            min_duration: 1.5    # minimum length, in seconds
            max_duration: 10.0   # maximum length, in seconds
            bit_depth: 16        # exact PCM bit depth
            min_rms_db: -60      # minimum RMS level, in dBFS
          "Output Labels":
            min_labels: 1        # minimum labels in a pyharp LabelList
```

The following is the full vocabulary along with which output types each rule covers:

| Rule | Applies to | Asserts |
|---|---|---|
| `ext` | any file output | Extension matches (string, or list of accepted extensions) |
| `min_bytes` | any file output | File is at least this many bytes |
| `num_channels` | audio output | Exact channel count |
| `sample_rate` | audio output | Exact sample rate in Hz |
| `bit_depth` | audio output | Exact PCM bit depth (16, 24, ...), and errors on compressed formats such as MP3 or OGG |
| `min_rms_db` | audio output | RMS level is at least this many dBFS |
| `min_duration` / `max_duration` | audio or MIDI output | Length in seconds is within bounds |
| `min_notes` | MIDI output | At least this many note-on events |
| `note_instruments` | MIDI output | Notes use only these General MIDI program numbers (one, or a list of accepted) |
| `note_channels` | MIDI output | Notes use only these channels (one, or a list of accepted) |
| `min_labels` | JSON (`LabelList`) output | At least this many labels returned |

`note_instruments` and `note_channels` assert coverage rather than presence.
Every instrument or channel the notes carry has to be one the rule lists, and
the rule may list values the file never uses. A file with no notes satisfies
both, since requiring notes is what `min_notes` is for. They name the notes
because a MIDI file has no single instrument or channel of its own, only notes
that each use one.

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
aimed at a named output whose type it does not cover (_e.g._ `min_labels` on an audio
output) fails the case with a message naming the problem and listing what
is valid. The run still exits `1`, since the case did not pass, but it is the
config that needs the fix. A rule under `"*"` is more forgiving. When it
matches no compatible output it is skipped instead of raising an error. This
is what allows a generic case (see
[common test cases](#step-4-reuse-a-case-across-models)) to target output types a
given model does not have.

**On `min_rms_db`:** level is measured as RMS in dBFS, where 0 dBFS is full
scale. It is a bit-depth-independent unit that is easy to set thresholds in.
Digital silence is `-inf`, quiet noise floors sit near `-60`, and typical
program material is above `-40`. RMS is also more robust than a peak
measurement, since a single stray click cannot make an otherwise-silent file
pass. `min_rms_db: -60` is the usual check for silence.

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
instead. That counts as a skip rather than a failure, which is what allows a
validator to be used in a common test case.

The shipped `labels_within_audio` does both. It asserts that every returned
label falls inside the audio output's timespan, a relationship between two
outputs that no single-output rule can express, and it opts out when the model
has no audio output or no label output.

Add custom validators to [validators.py](src/validators.py), registered with the `@validator` decorator
under the name test cases will reference:

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
`expect` vocabulary (`EXPECT_RULES` in [expectations.py](src/expectations.py)) instead of
putting it in a one-off validator.

### Step 4: reuse a case across models

A case under a model's `test_cases` only applies to that model. For a check
that should hold for *every* model, such as "no file output is empty" or
"audio is never silent", add a `common_test_cases` entry at the top level of
config.yml instead. Common cases run on every model in addition to any model-specific tests,
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
audio models and is skipped on MIDI-only ones. A validator should do the same by
raising `ValidatorNotApplicable` when the model lacks the outputs it needs.
A model can opt out of common cases
entirely with `skip_common_cases: true` in its `overrides` entry.

### Step 5: run a single model to iterate

```bash
python model_validation/src/validate_models.py --spaces teamup-tech/pitch_shifter --verbose
```

The console shows each case's pass or fail, and `report.json` has the per-case
timings and error details.

### Reference: full config.yml schema

[config.yml](config.yml) documents every setting in one place, with each one
shown alongside its default. The top level takes `exclude`, `include_extra`,
`synthesized_inputs`, and `common_test_cases`. Each model's `overrides` entry
takes `connect_timeout`, `process_timeout`, `load_only`, `skip_common_cases`,
`synthesized_inputs`, and `test_cases`. A representative entry for each tier
is included as an example.

Three of those per-model settings also exist as CLI flags. A model's
`connect_timeout` or `process_timeout` is a fact about that model, so it wins
over `--connect-timeout` and `--process-timeout`, which set the value for
every other model in the run. `--load-only` works the other way around,
because it only ever restricts, so it holds for every model whether or not one
sets `load_only` itself.

A key outside these sets is reported as a configuration error, so a slip such as `test_case` for `test_cases` is
caught before any model runs, instead of leaving the model on its default
case while the file appears to say otherwise.

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
