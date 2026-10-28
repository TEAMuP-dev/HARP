# HARP Model Validation

Automated validation of HARP model deployments. Verifies that each deployment
is reachable, exposes the HARP gradio endpoints (`/controls` and `/process`),
and can actually process test inputs end to end. These are the same
interactions the HARP client performs, driven headlessly. A separate tier reads
the files of each deployment and checks them against
[docs/DEPLOYMENT.md](../docs/DEPLOYMENT.md).

## Overview

Three tiers share the configuration, and the first two share the test
harness:

- **Examples** (`--local-examples`) launches the apps under `pyharp/examples/`
  locally. A failure means pyharp or gradio itself is broken.
- **Spaces** (the default) validates every Hugging Face Space under `--org`. A
  failure means that deployment is broken, by a build error, a platform change
  or dependency drift, for example.
- **Lint** (`--lint`) reads every Space's files from the Hub without running
  it. A failure means the Space departs from the deployment guide in a way no
  run can show, such as loading its model twice or pinning a requirement that
  will not resolve on its next rebuild. See [Linting Spaces](#linting-spaces).

Key behaviors:

- **Models are validated independently.** A crash, hang, or timeout in one
  never stops the rest. All results are collected into a single report, and
  the process exits non-zero only after everything has run.
- **Crashed and stopped spaces are restarted automatically**, and a sleeping
  space is woken by a request that waits out its start. Pass
  `--no-restart-failed` to disable the restart. This is also what allows a
  run to work with a read-only token.
- **ZeroGPU time is tracked** across the run and reported after each ZeroGPU
  model, which includes a frontend on other hardware that declares the
  `zerogpu` tag for the backend to which it forwards. Pass `--skip-zerogpu` to skip
  these models and avoid spending allowance.
- **Models are asked to refuse what they cannot use.** A model taking audio or
  MIDI is also sent an unreadable file, and one taking audio an empty one, and
  passes only if it refuses each with a `gr.Error` before reserving the GPU.
  See [Expecting an Error](#expecting-an-error).
- **A Space's README is compared with its model card**, which costs nothing and
  so runs with `--load-only` too. See [README and Model Card](#readme-and-model-card).
- A daily GitHub Action
  ([model_validation.yml](../.github/workflows/model_validation.yml)) runs
  all three tiers. See [CI Behavior](#ci-behavior) for what it reports and for
  when a run turns red.

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
  write access to the Spaces of the organization. That access is what the
  default restart of crashed spaces needs. Read access is enough with `--no-restart-failed`.
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

# Only the named cases, here the refusals, which spend no ZeroGPU allowance on
# a model refusing as it should. A common case can drop its "common:" prefix
python model_validation/src/validate_models.py --cases refuses-unreadable-audio refuses-empty-audio

# Lint tier: check the files of each Space against docs/DEPLOYMENT.md, running nothing
python model_validation/src/validate_models.py --lint
python model_validation/src/validate_models.py --lint --spaces Demucs

# Examples tier (needs pyharp + example deps, no token required)
pip install torch torchaudio --index-url https://download.pytorch.org/whl/cpu
pip install -e ./pyharp
python model_validation/src/validate_models.py --local-examples
python model_validation/src/validate_models.py --local-examples pitch_shifter
```

**Naming models.** `--spaces` names Hugging Face Spaces and `--local-examples`
names local examples. Neither accepts the models of the other. Both expand a bare
name for you, so `pitch_shifter` means `teamup-tech/pitch_shifter` for
`--spaces` and the `pyharp/examples/pitch_shifter` directory for
`--local-examples`. Write `<owner>/<name>` on `--spaces` to reach another
organization, or give `--local-examples` a path to an example kept outside
`pyharp/examples/`.

## Selecting Models

A spaces run validates every space under `--org`. Two keys in
[config.yml](config.yml) adjust that standing set in either direction.

`include_extra` adds spaces from outside the organization, _i.e._ a HARP
deployment kept under another account. It describes discovery, so it
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

## Linting Spaces

A run judges what a Space returns, and many departures from the deployment
guide still return the right outputs. A model loaded in both processes, a GPU
reserved before the input is checked, and a requirement that resolves today
but not on the next rebuild all pass every case. `--lint` reads each Space's
files from the Hub instead and checks them against
[docs/DEPLOYMENT.md](../docs/DEPLOYMENT.md), without starting anything. It
selects Spaces the way a spaces run does, needs the token only to read them,
and spends no ZeroGPU allowance.

Each finding names its rule and the section of the guide from which it comes. An
**error** is a rule the guide states, and it fails the Space. A **warning** is
a risk the guide leaves to judgment, and it is reported without failing
anything. The rules on the app file, which is the README's `app_file` or
whatever a Docker Space's CMD or start script runs, are all errors but the
last:

- `spaces-first`: `spaces` is imported before any third-party module, since
  on ZeroGPU it has to come before anything that initializes CUDA.
- `spaces-fallback`: `spaces` is not imported inside a `try`, as a fallback for
  it missing, since it is always installed.
- `gpu-on-process-fn`: `@spaces.GPU` is not on the function passed as
  `process_fn`, which checks the input before the GPU is reserved.
- `load-outside-worker`: no model is loaded or downloaded at module scope,
  directly or through a function or thread the module starts, nor in the
  `__main__` block. Module scope runs in the app as well as the worker, and
  the `__main__` block in the app alone, which never runs `process_fn`.
  Importing one of the Space's own modules that loads when imported counts as
  loading at module scope.
- `load-in-gpu-call`: on ZeroGPU, no GPU function loads a model, directly or
  through the Space's own functions, nor moves a model to the device. ZeroGPU
  runs each call in a fresh fork, so either is repeated on every call, whatever
  cache holds the model. A function the app already ran at startup counts as a
  cache the fork inherits. Moving a model again after a loader that placed it
  costs nothing, so it is not reported.
- `interface-outside-main`: no component, `build_endpoint`, or `launch` runs
  outside `if __name__ == "__main__":`.
- `card-order`: the `ModelCard` is at module scope, ahead of the model loading
  and `process_fn`.
- `show-error`, `queue`: `launch` is given `show_error=True`, and
  `demo.queue()` is called.
- `info-warning`: no `gr.Warning` or `gr.Info`, which never reach HARP.
- `input-format`: no `gr.Audio` input sets `format`, which converts every
  upload before `process_fn` sees it.
- `aiff-container`: no set of kept containers lists `.aiff` or `.aif`. Gradio
  converts an AIFF output to 16-bit WAV, and soundfile cannot write a file
  named `.aif` at all, so an AIFF input comes back as WAV. This one reads every
  top-level module.
- `unpinned-revision` (warning): weights fetched from a Hub repository named
  in the code pin a `revision`.

An app file that is missing or does not parse fails as `app-file`, before any
of these can be checked. The two loading rules follow calls into the Space's
other top-level modules, such as a `runtime.py` beside the app, but not into
packages it vendors in subfolders. They know a loader by its name
(`from_pretrained`, `hf_hub_download`, `load_model`, and others listed in
`LOADER_NAMES`), so a model built by a constructor of any other name, such as
`VoiceFixer()`, is not found.

The rules on what a Space installs read `requirements.txt` and
`pre-requirements.txt`, or for a Docker Space whatever its Dockerfile
installs:

- `pyharp-pin`: pyharp comes from the TEAMuP-dev repository at a release tag
  or a commit, not a branch.
- `gradio-listed`: a Gradio Space does not list `gradio`, which `sdk_version`
  pins.
- `spaces-listed`, `spaces-exact`: an app importing `spaces` installs it, with
  a lower bound rather than an exact pin.
- `zerogpu-torch`: an exact `torch` pin on ZeroGPU is a release ZeroGPU
  accepts.
- `torchaudio-match`: the torchaudio installed loads beside the torch
  installed (see below).
- `url-unpinned`: a requirement given by URL names a commit or a release,
  unless a comment beside it says why it cannot.
- `resolves`: the requirements resolve, for the Python with which the Space
  builds.
- `unpinned` (warning): every other requirement is pinned to one version. The
  guide asks this of the package implementing the model only, so the rest is
  reported, not failed.

**Resolution.** The `resolves` rule runs `uv pip compile` on the requirements
the way the Space's build would install them, without installing anything. A
Gradio Space is resolved beside what the SDK image installs, for its README's
`python_version`, or else for the default the platform uses, which is 3.10 on
ZeroGPU and 3.13 elsewhere. A Docker Space is resolved only where the pip
of its base image installs a requirements file, for the Python of its
`python:X.Y` image, since its other installs run one after another in an order
a single resolution cannot reproduce. uv is listed in
[requirements.txt](requirements.txt), and a lint run without it stops with
exit code `2`. The torch and torchaudio the resolution picks are what
`torchaudio-match` compares. torchaudio 2.11, its final release, declares no
torch requirement and loads only beside torch 2.11 or later, so a Space that
pins an older torch and leaves torchaudio to pyharp's dependencies resolves
without complaint and then fails to start.

**Exceptions.** A Space that departs from a rule for a reason records it in
its `overrides` entry, keyed by the rule, with the reason as the value:

```yaml
overrides:
  teamup-tech/MuScriptor:
    lint_exceptions:
      load-in-gpu-call: muscriptor reads its safetensors straight onto CUDA, which
        ZeroGPU's emulation does not answer outside a call.
```

An exception that no longer matches a finding is reported as a
`stale-exception` warning, so that it is removed rather than left to excuse a
later regression.

The report lists each Space's errors and warnings in place of test cases, and
the exit codes are those of the other tiers.

## Run Output

Each run writes to its own timestamped directory under `model_validation/reports/`.
Override the base with `--output-dir`. A run directory looks like this, with
the `assets/` and `examples/` directories left out by a lint run:

```
model_validation/reports/2026-07-18T14-30-00/   (local time)
├── report.json          machine-readable results
├── report.md            results table, with full error text below it
├── assets/              the test inputs synthesized for this run
└── examples/            examples tier only, one directory per example
    └── pitch_shifter/
        ├── app.log      the stdout and stderr of the example
        └── _outputs/    whatever the example itself wrote
```

Both reports record the command line that invoked the run. Exit code is
`0` when everything passes, `1` on any model failure, and `2` when the run
could not proceed at all (missing token, no models found, bad configuration).

Each example runs in its own directory under `examples/` rather than in its
source directory. pyharp writes model outputs to an `_outputs` folder under the
working directory, so this keeps the files of a run together and leaves the
pyharp checkout untouched.

## ZeroGPU Usage Tracking

The line of each model shows its hardware. ZeroGPU models additionally show the
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
for want of an input, or that never left the queue, does not count. Nor does a
case expecting an error that the model refused, since its own checks refused
it before any GPU was reserved, while one whose error shows the GPU was
reached, such as a ZeroGPU worker error, does. CPU and dedicated-hardware
models never contribute.

**Wall time** is how long those calls took, measured from submission to
result. Treat it as a rough ceiling on the GPU time a run costs rather than as
the bill, for two reasons:
1. It includes the queue wait before the job ran, which is not GPU time.
2. ZeroGPU charges dynamically. A call reserves its
declared `@spaces.GPU(duration=...)` up front, then refunds the unused part
when the function returns, so the settled amount is usually lower.

The wall time of each case is also recorded in `report.json` as `duration`.
All of these figures describe the work this run does. None of them say
anything about the allowance remaining on the account, which Hugging Face
reports only to a token with billing permissions. To spend none of the allowance, pass `--skip-zerogpu`.
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

## Spaces That Will Not Start

A sleeping space wakes automatically when something requests it.
However, sometimes this does not work and the host answers at once with a `503`
and an empty body, and the stage stays `SLEEPING` for however long the request is
held open or for however often it is repeated. Authenticating the request makes no
difference, and neither does loading the page of the space.

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

Failures that are not the fault of the model are retried rather than reported.

A "model is still loading" response comes from a space starting its GPU
worker. It is retried until `--connect-timeout`, the time budgeted for a model
to start.

An infrastructure fault is retried `TRANSIENT_RETRY_LIMIT` times at a short
interval instead. Currently covered are a read timeout, a GPU host ECC error,
and a server disconnect. None of these has an expected duration to wait out,
which is why they are bounded by a retry count rather than by a deadline, and
the retry usually surfaces the real behavior of the model.

This list is `TRANSIENT_MARKERS` in [src/harness.py](src/harness.py), and it
is deliberately narrow. A retried `/process` call reserves ZeroGPU allowance
again, so a marker that also fires on a genuine failure multiplies what that
failure costs and delays the real error. It is why a dropped connection is not
matched broadly, since a model that crashes its own Space can drop the connection
in exactly the same way. Add an entry only once a fault is confirmed to clear
on a retry.

## README and Model Card

HARP lists a sleeping model on the Home tab from its README alone, and shows
its model card once it is loaded, so the two have to agree.
[docs/DEPLOYMENT.md](../docs/DEPLOYMENT.md) asks for the README's `tags` to be
the full tag list of the card, inferred `input:` and `output:` tags included,
and for its `title` to be the name of the card. Nothing else reconciles them, so a spaces
run compares them once `/controls` has answered, and a difference fails the
model with a `readme` entry naming the tags missing from either side. The
comparison needs no inference, so it runs with `--load-only` as well. A local
example has no README and is not compared.

## Configuring Test Cases

Every model automatically gets one **`default`** test case, even with no
configuration at all. Its inputs are synthesized from the `/controls` spec of
the model. Audio tracks get a clip cut from a recording, MIDI tracks get a MIDI file
(both from [test_data/](test_data/), see below), and sliders, toggles,
dropdowns, number boxes, and text boxes get the default value each one
declares. A control that declares no default falls back to
something valid for its type, such as its minimum or its first dropdown
choice, so every model can be exercised without configuration.

Configuration is only needed to go beyond that: pinning specific control
values, feeding real audio, adjusting the properties of the synthesized
inputs, or inspecting outputs more deeply.

### Synthesized Input Properties

Without configuration, the generated inputs are a 2-second mono 44.1 kHz sine
sweep and a two-note MIDI file. A `synthesized_inputs` block changes them, at
the top level of [config.yml](config.yml), per model under `overrides`, or per
test case. Each level overrides the one before it, and a block sets only the
properties it names.

A `source` replaces the sweep or the notes with real material. An audio source
is resampled to `sample_rate`, averaged or copied to `num_channels`, and cut or
repeated to `duration`. A MIDI source is passed as it is. A sweep only shows
that a model returns something. Real material shows whether it did its job,
since a separator then has sources to separate and a transcriber notes to find.

The shipped config gives every model eight seconds of a band (drums, bass,
piano and a lead) under a speaking voice, at 48 kHz in stereo, and a monophonic
melody for MIDI. Speech models take English speech in mono at 44.1 kHz instead,
so both shapes of input are exercised. A model that returns its input,
processed or not, asserts the sample rate, channel count, and container of the
input. Few models run at 48 kHz stereo, so one that writes at its own rate
or in mono fails. The band is sent as WAV, which is what a DAW hands HARP, and
the speech as FLAC. A lossless input keeps its container, which WAV alone could
not show, since a model that always writes WAV would pass. FLAC is the one
lossless container besides WAV that Gradio returns as it is, since it converts
an AIFF output to 16-bit WAV. The assets, made for this suite, are described at
the top of config.yml.

```yaml
# config.yml, at the top level, so it applies to every model
synthesized_inputs:
  audio:
    source: test_data/test_piano_20s.wav  # default: none, a sine sweep
    sample_rate: 48000   # default 44100
    num_channels: 2      # default 1
    duration: 5.0        # default 2.0
    ext: .flac           # default .wav
    content: unreadable  # default: none. An input to refuse, see below
  midi:
    source: test_data/test_melody_16s.mid  # default: none, synthesized notes
    num_notes: 8         # default 2, ignored with a source
    note_duration: 0.25  # default 0.5, ignored with a source
    instrument: 24       # default 0, ignored with a source
    channel: 0           # default 0, ignored with a source
    content: unreadable  # default: none. An input to refuse, see below
```

`content` replaces the input with one a model cannot use, for a case that
[expects an error](#expecting-an-error). `unreadable` is a file with the
right extension that holds no audio or MIDI at all, and `empty` is a valid file
holding nothing: an audio file of no frames, written as WAV whatever `ext`
says, since a FLAC file of no frames cannot be opened, or MIDI without a note.

The MIDI `instrument` is the General MIDI program number, written as a program
change, from which both HARP and a model read the instrument of a note.
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
varying, and one accepting some other format has to be supplied by the
`files` entry of a test case. An unrecognized type or property name is reported as a
configuration error before any model runs, rather than being read by nothing
at all.

Scalar controls have nothing to do with this block. A text box, number box,
slider, toggle, or dropdown takes the default value its `/controls` spec
declares, and the `controls:` entry of a test case is what overrides it. (A JSON
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
file input it is a preference instead, since the `file_types` a component
declares are a contract and one `ext` setting spans every model. A
component accepting only other formats is given one it accepts, and one whose
accepted types cannot be synthesized at all, such as a bespoke binary format,
has its case skipped with a message naming the input. When a specific input
needs a specific file, name it in the `files` entry of a test case. A model whose
every case is skipped reports `SKIP` rather than `PASS`, since `/process` was
never called.

A file for each distinct set of properties is generated once and reused for
the rest of the run.

### 1. Find the Control Labels

Test cases reference inputs and outputs by their **label**, the display name
each gradio component was given in the `app.py` of the model. There are three ways
to find them:

- Open the Space's gradio UI and read the component titles, or click its
  "View Controls" button to see the full spec as JSON.
- Read the `app.py` of the model, where each component has a `label=...`.
- Reference a wrong label on purpose and run the validator. The error message
  lists every available label.

### 2. Add the Case

Cases live under `overrides:`, keyed by the qualified name of the model, which
is its space id or `examples/<example-dir>`. Each case needs a `name` and takes
several optional fields.

`controls` overrides scalar values by label, and
`files` substitutes input files by label, with paths taken relative to
config.yml (real inputs belong under `test_data/`).
`process_timeout` can be overridden at either the model or case level.
See above for information about `synthesized_inputs` and [Check the Outputs](#3-check-the-outputs) for information about `expect` and `validators`.

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

An input the model marks as optional is left empty rather than synthesized,
since that is what a request without it sends. A case that needs one, such as
a reference voice or a melody to follow, names it in `files`.

A model with several modes gets a case for each mode that runs different code
or loads different weights, since one mode can break while the others work. A
mode that changes only a number on the same path is covered by the default
case. Every case is a `/process` call, so on ZeroGPU each extra case is
weighed against the time and allowance a run spends (see
[CI Behavior](#ci-behavior)).

### 3. Check the Outputs

Every case already gets structural checks for free. `/process` must not error,
file outputs must exist and be non-empty, and a JSON output (_e.g._ an optional
pyharp `LabelList`) must be well-formed when present. Note that an absent or `None`
label output is valid, since labels are optional.

Beyond that, there are two mechanisms to validate output:

- **`expect`** asserts properties of a *single* output, and covers most
  checks.
- **`validators`** run custom Python for checks that cannot be expressed as a
  property of one output, typically relationships spanning several outputs.

#### Declarative Rules

Keyed by output label, then by rule. Every rule is optional, and an output
with no rules is still subject to the default structural checks:

```yaml
      - name: extreme-shift
        controls:
          "Pitch Shift (semitones)": 24
        expect:
          "Shifted Audio":
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

The full vocabulary, by the outputs each rule applies to:

- **Any file output**: `ext` requires the extension (one, or a list of accepted ones), and
  `min_bytes` a least size in bytes.
- **Audio outputs**: `num_channels` and `sample_rate` require exact values. `bit_depth` requires an
  exact PCM bit depth (16, 24, ...), and errors on compressed formats such as MP3 or OGG.
  `min_rms_db` and `max_rms_db` bound the RMS level in dBFS. `max_clipping` caps the fraction of
  samples sitting in flat tops at full scale (0.001 is 0.1%). `min_duration` and `max_duration`
  bound the length in seconds, for MIDI outputs as well.
- **MIDI outputs**: `min_notes` requires at least that many note-on events. `note_instruments`
  lists the General MIDI program numbers the notes may use, and `note_channels` the channels (one
  value, or a list, for either).
- **Label outputs** (a JSON `LabelList`): `min_labels` requires at least that many labels.
- **Text file outputs**: `min_length` requires at least that many characters, ignoring surrounding
  whitespace, and `excludes` forbids text (one string, or a list), matched case-insensitively.

`note_instruments` and `note_channels` assert coverage rather than presence.
Every instrument or channel the notes carry has to be one the rule lists, and
the rule may list values the file never uses. A file with no notes satisfies
both, since requiring notes is the purpose of `min_notes`. They name the notes
because a MIDI file has no single instrument or channel of its own, only notes
that each use one.

**Targeting outputs.** A model with several outputs gets one block per output
label. Use `"*"` instead of a label to apply rules to every output a rule
covers. With mixed outputs, `"*"` sends `min_bytes` to all file outputs,
`min_rms_db` only to the audio ones, and `min_notes` only to the MIDI ones:

```yaml
        expect:
          "*":
            min_bytes: 1000      # every file output must be non-trivial
```

A rule under the label of an output replaces the same rule under `"*"` for that
output, whichever key comes first. `"*"` can therefore set a rule for every
output while a label makes the exception for one, and the other rules under
`"*"` still reach it.

**Mistakes on a named output are surfaced as configuration mistakes, not as
model faults.** An unrecognized rule name, an unknown output label, or a rule
aimed at a named output whose type it does not cover (_e.g._ `min_labels` on an audio
output) fails the case with a message naming the problem and listing what
is valid. The run still exits `1`, since the case did not pass, but it is the
config that needs the fix. A rule under `"*"` is more forgiving. When it
matches no compatible output it is skipped instead of raising an error. This
is what allows a generic case (see
[common test cases](#4-reuse-a-case)) to target output types a
given model does not have.

**On `min_rms_db`:** level is measured as RMS in dBFS, where 0 dBFS is full
scale. It is a bit-depth-independent unit in which thresholds are easy to set.
Digital silence is `-inf`, quiet noise floors sit near `-60`, and typical
program material is above `-40`. RMS is also more robust than a peak
measurement, since a single stray click cannot make an otherwise-silent file
pass. `min_rms_db: -60` is the usual check for silence.

`max_rms_db` is the converse, for an output that should be near-silent. The
stem of a separator for a source its input does not contain is one use: it
shows that the model separated rather than copying its input into every stem.
Assert one only on a source the input truly lacks, measured, since a model can
also route part of a source to a neighboring stem. An output given a
`max_rms_db` usually needs `min_rms_db: -.inf` as well, to lift the common
silence floor.

**On `max_clipping`:** a waveform that ran over full scale and was cut off at
the ceiling stays flat there, for longer the further it ran over, and what was
cut off cannot be restored. Samples at full scale alone do not show that,
since a tone normalized exactly to full scale reaches the ceiling at the top
of every cycle. So only runs at full scale lasting at least 0.2 ms count,
which the crest of any tone of 20 Hz or above stays short of, at any sample
rate. Full scale is the largest value 16 bits can hold, so a 16-bit file cut
off at its ceiling counts, and so does a float file whose samples run over
1.0, which will clip wherever it is converted. The measured fraction and the
peak level are recorded for every audio output in the report, asserted or not.

**On `min_labels`:** `min_labels: 0` is meaningful, and is the right rule when
a model may legitimately return no labels. It asserts that a well-formed
`LabelList` came back while permitting it to be empty. Omitting the rule
entirely is weaker, because a missing or `None` label output also passes.
Labels are optional in pyharp.

**On text outputs:** a model returning text writes it to a
`gr.File(file_types=[".txt"])`, which pyharp reports as a generic file, since
it renders a `gr.Textbox` as an input control and refuses it as an output. The
structural checks require that file to exist and be non-empty, which a file
holding only whitespace satisfies, so `min_length: 1` is what asserts the
model actually said something. `excludes` covers the deployment that reports
its own failures as text instead of raising `gr.Error`, so that the result
arrives as a success and only its content distinguishes it from one. Neither
rule matches a pattern: a check on what the text *says* belongs in a
validator.

**On audio formats:** audio outputs are decoded with
[soundfile](https://python-soundfile.readthedocs.io/), a listed dependency, so
audio rules work on any libsndfile format. That covers WAV, FLAC, OGG, MP3,
AIFF, and more, not just pyharp's `save_audio()` WAV default. Decoding to
float also makes every audio rule bit-depth-independent, so a level threshold
means the same thing whether the source is 16-bit, 24-bit, or float.
`bit_depth` is the exception, since it reads the PCM encoding of the file and
errors on compressed formats, which have no fixed bit depth.

**On MIDI:** MIDI outputs are parsed with
[mido](https://mido.readthedocs.io/), also a listed dependency.
`min_duration` and `max_duration` are shared with audio and read the MIDI's
own length in seconds. `min_notes` counts note-on events.

#### Custom Validators

Registered in [validators.py](src/validators.py) and referenced by name, with
the parameters of each validator nested beneath it so they stay separate from
the fields of the test case. A case may run several:

```yaml
      - name: labels-line-up
        validators:
          labels_within_tracks:
            tolerance: 0.1
```

A validator is a function taking three arguments:

- `outputs`: each output label mapped to its value. A file output is a local
  path, and a JSON output is the decoded object.
- `controls`: the full `/controls` payload (card, inputs, outputs).
- `params`: the settings nested under the name of this validator in the test
  case.

To report a problem, a validator raises `AssertionError`. The message should
name the output at fault and say what was wrong with it. To opt out on a model
that lacks the outputs it needs, a validator raises `ValidatorNotApplicable`
instead. That counts as a skip rather than a failure, which is what allows a
validator to be used in a common test case.

The shipped `labels_within_tracks` does both. It asserts that every returned
label falls inside the timespan of the track over which HARP draws it, a
relationship between two outputs that no single-output rule can express, and
it opts out when the model has no audio or MIDI output or no label output.

Six validators ship with the harness:

- `audio_decodes`: every audio output decodes and carries frames. Skips a model
  that returns no audio.
- `declared_format`: a model given no audio declares the sample rate and
  channel count of the audio it returns, in the tags of its card. Skips a model that
  takes audio or returns none.
- `labels_reach_harp`: every JSON output is a pyharp `LabelList`, which HARP
  requires, and the model has an audio, MIDI or file output on which the labels
  can appear. Skips a model with no JSON output.
- `labels_within_tracks`: every label falls inside the timespan of the track
  of its kind, an `AudioLabel` over the audio output and a `MidiLabel` over the
  MIDI output, within `tolerance`. Skips a model with no audio or MIDI output or
  no `LabelList`.
- `merit_scores`: MERIT's three cosine similarities are finite and within
  [-1, 1]. Never skips.
- `muq_ranking`: MuQ returns `count` candidates, numbered and sorted by
  similarity. Never skips.

The first four are model-agnostic and run as
[common test cases](#4-reuse-a-case). The last two name
MERIT's and MuQ's own outputs, so they are referenced from those models'
entries. The local regression checks cover all of them, plus the `expect`
rules and config.yml itself:

```bash
python -m unittest discover -s model_validation/tests -v
```

These need no token and no network.

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

### 4. Reuse a Case

A case under the `test_cases` of a model only applies to that model. For a check
that should hold for *every* model, such as "no file output is empty" or
"audio is never silent", add a `common_test_cases` entry at the top level of
config.yml instead.

Every case costs a `/process` call, which on ZeroGPU reserves allowance, so a
common case that only checks outputs does not run on its own. Its `expect`
rules and `validators` are merged into each case of the model, whose own
settings win where the two conflict. A common case that sends
inputs of its own (`controls`, `files`, or `synthesized_inputs`) is a
different scenario, so it runs separately, named with a `common:` prefix in
the report. It takes whatever inputs the first case of the model sends and it
does not set, so a model that needs particular files still gets them.

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

Nine common cases ship in [config.yml](config.yml), so that a model with no
entry of its own is still asserted against rather than only run.
`audio-decodes` decodes every audio output, which is what catches a
header-only WAV from a model that wrote no samples and reported success,
`labels-within-tracks` keeps labels inside the track they annotate,
`labels-reach-harp` fails a JSON output HARP would reject,
`outputs-carry-signal` fails digital silence and MIDI with no notes,
`audio-unclipped` fails audio that clipped, and `declared-format` fails a model
that is given no audio and returns audio its card does not describe. The other
three are refusals, described under
[Expecting an Error](#expecting-an-error).

`declared-format` exists because a model given no audio makes its own, at a
rate and channel count the model fixes, which
[docs/DEPLOYMENT.md](../docs/DEPLOYMENT.md) asks its card to declare with
`SampleRate` and `Channels`. Nothing infers those tags, and a user choosing a
model for a session reads them. A model given audio returns the format of its
input instead, which the format assertions check, so the validator skips it.

The silence floor is set at -80 dBFS on purpose, well below any real level, to
catch digital silence without judging loudness. A real level is asserted per
model instead, where producing signal is part of the contract. The stem of a
separator for a source its input does not contain is legitimately quiet, at times
below even this floor, so such a stem lifts the floor by name and asserts its
silence with `max_rms_db` (see above). The same test applies to anything
proposed as a common case: a rule that holds for most models reads as a
failure on the rest.

Keep common cases model-agnostic by targeting outputs with `"*"` rather than a
specific label, which will not exist on most models. Because a `"*"` rule is
skipped when it matches no compatible output, `audio-not-silent` above checks
audio models and is skipped on MIDI-only ones. A validator should do the same by
raising `ValidatorNotApplicable` when the model lacks the outputs it needs.
A model can opt out of common cases entirely with `skip_common_cases: true` in
its `overrides` entry, or out of the ones it names with a list, such as
`skip_common_cases: [refuses-empty-audio]`, beside a comment saying why.

### Expecting an Error

A case with `expect_error` sends an input the model cannot use, and passes
only if the model refuses it the way [docs/DEPLOYMENT.md](../docs/DEPLOYMENT.md)
asks, with a `gr.Error` saying what was wrong. HARP shows that message to the
user. Any other exception reaches HARP as its bare text, such as "list index
out of range", which reads as a crash. On ZeroGPU the check also has to come
before the GPU is reserved, or a refused request still spends the allowance of
the caller.

`expect_error: true` accepts any such refusal, and text in its place requires
the message to contain it, matched case-insensitively. A case expecting an
error returns no outputs, so an `expect` or `validators` entry beside it is a
configuration error.

```yaml
      - name: over-the-limit
        controls:
          "Text to Speak":
            text_length: 5001     # 5001 characters of English text
        expect_error: "5000"      # the message has to name the limit
```

A control given as `{text_length: N}` sends N characters of English text, for
a model with a limit on its text. An audio input past a length limit takes a
`synthesized_inputs` duration beyond it, and `content` (see
[Synthesized input properties](#synthesized-input-properties)) sends a file no
model can use. The shipped config gives most models with a stated limit a
case just past it, in mono at 16 kHz to keep a long file small, which is what
tests the check that keeps such a request off the GPU.

The case fails when `/process`

- returns outputs, since the model then processed an input it should have
  refused.
- fails with anything but a `gr.Error`, which is how an uncaught exception
  reaches HARP.
- fails with an error ZeroGPU or pyharp raised rather than the model. A ZeroGPU
  worker error means the input was only found wanting inside the GPU call, a
  quota error means the request got as far as asking ZeroGPU for a GPU, and a
  pyharp "Processing failed" means the worker died.
- refuses with a message that says nothing a user can act on, such as the type
  name of an exception alone.
- does not answer within the timeout of the case.

Telling a `gr.Error` from any other exception takes the error payload Gradio
sends, which carries a `title` for a `gr.Error` alone, so the harness records
which fields arrived (see [refusals.py](src/refusals.py)). A `gr.Error` raised
inside the GPU call passes through ZeroGPU with its title, so it passes this
check as well, although the GPU was reserved for it. Where a check sits only
the code can show.

Cases expecting an error run after the other cases of a model, so that a worker
still loading after a restart answers a real request first, rather than holding
up a refusal past its timeout. A case whose `content` targets an input kind
the model does not take is left out, like a `"*"` rule matching no output. The
file it sends replaces every required input of that kind, including the files
a common case inherits from the first case of the model.

Three ship as common cases. `refuses-unreadable-audio` and
`refuses-unreadable-midi` send a file with the right extension holding no audio
or MIDI, and `refuses-empty-audio` sends an audio file of no frames. A MIDI
file without notes is not among them, since a model continuing a prompt can
legitimately start from nothing. Each is one request, which a model refusing
as it should answers at once, without the GPU, and `--cases` runs them alone:

```bash
python model_validation/src/validate_models.py \
  --cases refuses-unreadable-audio refuses-empty-audio refuses-unreadable-midi
```

### 5. Iterate on One Model

```bash
python model_validation/src/validate_models.py --spaces teamup-tech/pitch_shifter --verbose
```

The console shows the pass or fail of each case, and `report.json` has the
per-case timings and error details.

### Full Schema

[config.yml](config.yml) documents every setting in one place, with each one
shown alongside its default. The top level takes `exclude`, `include_extra`,
`synthesized_inputs`, and `common_test_cases`. The `overrides` entry of each
model takes `connect_timeout`, `process_timeout`, `load_only`, `skip_common_cases`,
`synthesized_inputs`, `test_cases`, and `lint_exceptions`, and each test case
takes `name`, `process_timeout`, `controls`, `files`, `synthesized_inputs`,
`expect`, `validators`, and `expect_error`. A representative entry for each
tier is included as an example.

Three of those per-model settings also exist as CLI flags. The
`connect_timeout` or `process_timeout` of a model is a fact about that model, so it wins
over `--connect-timeout` and `--process-timeout`, which set the value for
every other model in the run. `--load-only` works the other way around,
because it only ever restricts, so it holds for every model whether or not one
sets `load_only` itself.

A key outside these sets is reported as a configuration error, so a slip such as `test_case` for `test_cases` is
caught before any model runs, instead of leaving the model on its default
case while the file appears to say otherwise.

A model written twice is reported the same way. YAML keeps the last of
several entries sharing a key and discards the rest in silence, so a model
configured in two places would run with only the settings written under the
second entry.

## CI Behavior

- **Schedule:** daily at 06:30 UTC. It can also be run manually from the
  Actions tab, with options to validate specific spaces, skip inference, or
  disable the default restart of crashed spaces.
- **Lint:** a third job lints every space, or the ones named, alongside the
  other two. It takes a few minutes and spends no allowance, and its report is
  a separate artifact.
- **Reports:** a markdown summary on each run, plus JSON and markdown
  artifacts. A failed Space's entry carries the end of its run log, where the
  traceback behind the failure usually is.
- **Duration:** approximately 25 minutes for the whole fleet, most of it the
  ZeroGPU calls, which run one at a time while the CPU models run alongside
  them. Those calls are also what a change to the cases mostly moves. The
  refusals add a request each, which a model refusing as it should answers in
  seconds, without reserving a GPU, though on ZeroGPU it still waits for the
  one-at-a-time slot. A
  successful call is billed for its GPU time, which is less than its wall
  time, against the daily allowance of a PRO or team account. The token CI uses
  therefore needs that tier, and little other ZeroGPU use on the same day.
- **Harness tests:** the examples job first runs the unit tests under
  `tests/`, which also check config.yml, so a configuration mistake turns the
  run red before any model runs.
- **Failure signal:** model failures keep the run green, because deployments
  are not yet stable enough for daily failure emails to be useful. They show
  up as warning annotations and in the run summary and report instead. No
  issues are opened and no emails are sent. Only infrastructure errors, such
  as a bad token or a discovery failure, turn the run red, since those mean
  validation itself has stopped working.
- **Opting back into notifications later:** remove the exit-code handling in
  the two `Validate` steps and the `Lint spaces` step of the workflow so that
  exit code 1 propagates. Failed runs then turn red and GitHub emails
  maintainers.
