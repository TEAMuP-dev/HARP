# Deployment

Guidelines for deploying a model into the HARP ecosystem as a Hugging Face Space. These are the
conventions the featured models under [`teamup-tech`](https://huggingface.co/teamup-tech/spaces)
follow, and the shape any deployment should take.
[PyHARP's README](https://github.com/TEAMuP-dev/pyharp/blob/develop/README.md) documents how to
build an app, and this document covers what to decide while doing it.

## Table of Contents

- [Licensing](#licensing)
- [Model Card](#model-card)
- [Space README](#space-readme)
- [Versions and Compatibility](#versions-and-compatibility)
- [Structure of app.py](#structure-of-apppy)
- [Interface](#interface)
- [Model Weights](#model-weights)
- [Error Reporting](#error-reporting)
- [Preserving the Input Format](#preserving-the-input-format)
- [Hardware Portability](#hardware-portability)
- [Dependency Conflicts](#dependency-conflicts)
- [Calling Another Space](#calling-another-space)
- [Keeping a Space Lightweight](#keeping-a-space-lightweight)

## Licensing

Settle this first, since it decides whether a model can be deployed at all.

**Work with no license cannot be deployed.** Anything unlicensed is all rights reserved by default,
whatever its visibility. A public GitHub repository with no `LICENSE` file grants nothing, and the
fact that weights are downloadable is not permission to redistribute or deploy them. Ask the
authors, and record the permission when it is granted.

**Set `license` in the Space README frontmatter to the license of the original code.**

Where the terms are custom, commit them as a `LICENSE` file and set `license: other` with a
`license_name`. This covers a permission the authors granted in writing, which is itself the terms.
Prefer it to leaving the field blank, since an omission is indistinguishable from an oversight.

HARP reads that field and shows it with the model, so a user sees the terms before running it.

**A missing `LICENSE` file is not the same as an unlicensed project.** Read the README's own
license section, and the other branches, before concluding that nothing was granted: a license is
often stated in prose, or committed only on the branch the released model was built from.

**Where code and weights carry different terms, declare the more restrictive of the two**, since
that is what binds whoever uses the output, and say in the description which part carries which.
Permissively licensed code loading non-commercial weights is common, and declaring only the code's
license misstates what a user may do with the result. Checking the license of every weight
repository a Space downloads from is part of deploying it.

### What Can Be Deployed

The categories below are a practical guide (not legal advice) for deciding whether a model can
be deployed through PyHARP. Where a license is unusual, restrictive, or ambiguous, confirm
what it permits before deploying.

| Category | Examples | Deploying |
|---|---|---|
| Permissive | `mit`, `apache-2.0`, `bsd-3-clause`, `isc`, `cc0-1.0`, `unlicense` | No concerns. |
| Weak copyleft | `lgpl-3.0`, `mpl-2.0`, `epl-2.0` | Fine. Obligations attach to modifications of the licensed code, not to a Space that calls it. |
| Strong copyleft | `gpl-3.0`, `agpl-3.0` | Fine to host. `agpl-3.0` treats network use as distribution, so the Space's source must be offered, which a public Space repository already satisfies. |
| Use-restricted | `openrail`, `openrail++`, `creativeml-openrail-m` | Fine, but the behavioral restrictions pass through to users. |
| Non-commercial | `cc-by-nc-4.0`, `fair-noncommercial-research-license` | Deployable as a demonstration, but users cannot use the output commercially. State this in the description. |
| No derivatives | `cc-by-nd-4.0` | Confirm what is permitted before deploying. Redistribution is allowed only unmodified, so the weights cannot be converted or repackaged and then hosted. |
| Community and gated | `llama3.x`, `gemma` | Acceptable, but do not mirror the weights. Download from the gated upstream repository so the terms are accepted by whoever runs it. |
| Unstated | no license at all | **Do not deploy.** See above. |

## Model Card

The `ModelCard` is what HARP shows in its model info panel.

### Name

Prefer the model's original name, as its authors published it (_e.g._ `Whisper`, `Demucs`, `Bark`).
A searchable name connects the Space to the paper and repository behind it.

Where the work has no name of its own, title it by what it does:

```python
name="Historical Recording Denoiser"
```

### Author

Credit the original authors of any relevant paper or work, if applicable. Otherwise, credit the
GitHub or Hugging Face account that first published the model.

### Description

Describe what the model **does**, in terms of the audio going in and coming out. This is a
producer-facing field, so what the model *is* and how it works belong in a linked paper rather
than here.

Include any detail of how the model was trained that could affect its performance:

- The sample rate it expects, and whether it resamples
- Whether it is mono or handles channels independently
- The material it was trained on, where that limits it (solo piano, speech, drum stems)
- Any hard limit on input length, and what happens past it

Link the paper and the original repository.

### Tags

HARP's Home tab categorizes and searches by a model's tags. PyHARP's
[Tags](https://github.com/TEAMuP-dev/pyharp/blob/develop/README.md#tags) section documents the kinds
and lists the taxonomy. The conventions:

- **Place the model in the taxonomy with `Category` and `Subcategory`**, listing the most specific
  entry that applies. A `Subcategory` implies its `Category`, so listing both is unnecessary.
- **Tag every task the model performs**, so that a user searching for either finds it.
- **Declare `SampleRate` and `Channels`** whenever the model is fixed to one. Nothing can infer
  them, and they tell a user whether the model suits their session.
- **The `input:` and `output:` tags are inferred** from the Gradio components. Providing one by
  hand is an error.
- **Describe anything else with a custom tag**, such as the model family or a notable feature.
  These are free-form strings, searched like any other tag.

## Space README

The Space `README.md` carries YAML frontmatter that decides how the Space builds and how it
appears on the Hugging Face Hub.
[UI-Tester](https://huggingface.co/spaces/teamup-tech/UI-Tester) is the reference:

```yaml
---
title: UI Tester
emoji: 🧪
colorFrom: indigo
colorTo: gray
sdk: gradio
sdk_version: 6.24.0
app_file: app.py
pinned: false
license: mit
short_description: Exercises every control, track, and label type in HARP.
tags:
  - category:utility
  - input:audio
  - input:midi
  - input:file/txt|csv|json|nam
  - input:text
  - output:audio
  - output:midi
  - output:labels
  - output:file
  - example
  - test
---

A PyHARP example which exercises every input control, track type, and output label type supported by HARP. Performs no real processing.
```

Field by field:

- **`title`** matches the `ModelCard` name, so that the same model is recognizable in both places.
- **`emoji`**, **`colorFrom`**, **`colorTo`** are the Space's thumbnail. Choose an emoji suggesting
  the task, and colors that are not jarring.
- **`sdk_version`** is pinned to `6.24.0`. This is the version the Space actually deploys with, and
  it must be set even though `gradio` does not appear in `requirements.txt`. Gradio `4.x` and
  earlier are incompatible with HARP, and versions before `6.13.0` cannot forward error messages at
  all.
- **`sdk`** is `gradio`, or `docker` for the layout under
  [Dependency Conflicts](#dependency-conflicts).
- **`app_file`** is `app.py`, and **`pinned`** is `false`.
- **`license`** is covered under [Licensing](#licensing) above.
- **`short_description`** and **`tags`** are covered under
  [Listing on the Home Tab](#listing-on-the-home-tab) below. With
  `title`, they are the only frontmatter HARP itself reads.

Below the frontmatter is the README body, which the Hub renders on the Space's own page. One or
two sentences is enough, since HARP does not use it.

### Listing on the Home Tab

`short_description` and `tags` place the model on the Home tab correctly. HARP lists every public
Space in the organization without waking them, so these two fields are all it has until a model is
loaded. Without them a sleeping Space still appears, under "Other" and by name alone. A Space the
Hub reports as private or disabled is left out of the listing entirely.

- **`short_description`** summarizes the `ModelCard` description within Hugging Face's limit of 60
  characters.
- **`tags`** is the model's full tag list in string form: its own tags and the inferred `input:`
  and `output:` ones.

**Copy that list rather than retyping it**, with `build_endpoint(..., show_controls=True)` and the
"View Controls" button, which shows it under `card`. Nothing reconciles these tags with the model
card, and unlike the card, a typo here raises no error. It leaves the model uncategorized for
anyone browsing.

## Versions and Compatibility

HARP and PyHARP are released as compatible pairs. **Pin `pyharp` to a commit** in
`requirements.txt`, never to a branch or a tag:

```
# pyharp v0.4.1
git+https://github.com/TEAMuP-dev/pyharp.git@<COMMIT>
```

A branch such as `develop` moves under the Space, so a rebuild can change how it behaves with
nothing in the repository having changed. A tag can move as well, whenever a release is corrected,
and that case is harder to see. pip caches the wheel it built for a URL, so a Space pinned to a tag
goes on installing whatever that tag named when the Space was last built, through later rebuilds and
factory reboots alike. Nothing in the installed metadata disagrees, since the version string is
whichever one the old commit declared. A corrected `v0.4.1` tag left 45 ZeroGPU Spaces on v0.4.0,
whose daemonic worker fails every `/process` call, while every pin still read `@v0.4.1`. A commit
cannot move, so the pin and the install cannot drift. Name the release in a comment above it, which
keeps the file readable without making the tag load-bearing. Gradio is pinned separately, by
`sdk_version` in the [Space README](#space-readme), and the two have to agree. The latest PyHARP
requires `gradio>=6.13.0,<7`, below which Gradio discards the payload carrying the reason a job
failed (see [Error Reporting](#error-reporting)).

So `requirements.txt` holds `pyharp` pinned to a commit, `spaces` (see
[Hardware Portability](#hardware-portability)), and the model's own dependencies, kept to what the
app imports (see [Keeping a Space Lightweight](#keeping-a-space-lightweight)). It does not hold
`gradio`.

HARP keeps any tag it cannot place and shows it as a custom tag, so [tags](#tags) degrade rather
than break. A Space deployed with a PyHARP older than the taxonomy still appears and still shows
whatever tags it declares, uncategorized under "Other", until it is redeployed with taxonomy tags.
The same holds in reverse, since HARP embeds `taxonomy.json` from PyHARP when it is built: a
category from a newer taxonomy shows as a custom tag on an older HARP.

## Structure of app.py

Follow the layout in
[PyHARP's README](https://github.com/TEAMuP-dev/pyharp/blob/develop/README.md), in this order:

1. Imports
2. `ModelCard`
3. Model loading at module scope
4. `process_fn`
5. `if __name__ == "__main__":`, and inside it:
   1. `gr.Blocks` with `input_components`, `output_components`, and `build_endpoint`
   2. `demo.queue().launch(...)`

Someone reviewing or fixing one Space should be able to open any other and find the same things in
the same places.

The requirements that are easy to miss, all of which HARP depends on:

- Every `gr.Audio` sets `type="filepath"`.
- Every `gr.File` carrying MIDI sets `type="filepath"` and `file_types=[".mid", ".midi"]`, or HARP
  renders it as a generic file picker rather than a MIDI track.
- The order of `input_components` matches the arguments of `process_fn`, and the order of
  `output_components` matches its return values.
- `demo.queue()` is called, or a running job cannot be canceled from HARP.
- `show_error=True` is set. See [Error Reporting](#error-reporting).
- A `gr.Audio` output sets `format` only where the model fixes the output format, since Gradio
  converts the result to it. Leaving it unset returns whatever `process_fn` wrote, which is the
  behavior [Preserving the Input Format](#preserving-the-input-format) requires.
- A generic `gr.File` output lists `file_types` where it returns one of a known set. PyHARP
  enforces them, raising a `gr.Error` if `process_fn` returns anything else.

### Worker Processes

PyHARP runs `process_fn` in a separate worker process, so that Cancel in HARP stops the work rather
than leaving it to finish on the Space. The worker reaches `process_fn` by importing `app.py`,
which runs everything at module scope a second time. That is why step 5 above puts the Gradio code
behind `if __name__ == "__main__":`, with `process_fn` defined above it. PyHARP's
[Worker Processes](https://github.com/TEAMuP-dev/pyharp/blob/develop/README.md#worker-processes)
section covers what this asks of `process_fn`. What it changes for a deployment:

- **Module scope runs in two processes**, the app and the worker, so a model loaded there is held
  twice. See [Model Weights](#model-weights).
- **`build_endpoint` takes `timeout_s`**, defaulting to 900 seconds. Raise it for a model that
  legitimately runs longer.
- **A Space that shells out should use `subprocess.run`.** The worker leads its own process group,
  so a cancellation then tears down what the job started instead of orphaning it.

## Interface

**Expose every parameter the model accepts.** One the model reads but the Space does not offer is a
choice made on the user's behalf, with no way to change it. Where a parameter has no useful range
to offer, fix it in `process_fn` and say so in the model card description rather than leaving a
control nobody can interpret.

**Describe every control.** HARP shows the description in its instruction panel while the control
is hovered, so the label has to stand on its own and the description carries the detail. GUI
controls take Gradio's `info`, while tracks and generic files take PyHARP's `.set_info(...)`:

```python
input_components = [
    gr.Audio(
        type="filepath",
        label="Reference Vocal",
    ).set_info("A dry solo vocal. Room tone or backing will be modeled along with the voice."),
    gr.Slider(
        minimum=0.0,
        maximum=10.0,
        value=3.0,
        label="Guidance Strength",
        info="How closely the result follows the prompt. Higher values track the text more "
             "literally and tend to sound less natural.",
    ),
]
```

This matters most where a name comes from the model's own code and means nothing outside it. Label
the control with what it does, and keep the original name in the description, where someone
comparing against the paper will look for it.

State the units, and what the extremes do. A range given without either leaves a producer guessing
at both.

PyHARP's README covers the rest of the interface:
[Gradio Endpoint](https://github.com/TEAMuP-dev/pyharp/blob/develop/README.md#gradio-endpoint)
lists the components HARP supports as controls and documents marking an input optional with
`.harp_required(False)`,
[MIDI Inputs & Outputs](https://github.com/TEAMuP-dev/pyharp/blob/develop/README.md#midi-inputs--outputs)
covers MIDI tracks, and
[Output Labels](https://github.com/TEAMuP-dev/pyharp/blob/develop/README.md#output-labels)
covers drawing labels over a track.

## Model Weights

Weights do not belong in a Space repository by default. Where they come from instead depends on
where they already live, how large they are, and whether their license permits redistribution. The
options below are in rough order of preference.

Whichever is used, resolve the weights **at module scope rather than inside `process_fn`**, so that
no job pays to fetch them, a failure to fetch surfaces when the Space boots, and a replacement
[worker](#worker-processes) is warm before the next request.

Module scope runs in both the app and the worker, so the model is held twice: host memory in either
case, and device memory as well on a dedicated GPU, where `.to(device)` also runs in both. Size the
hardware for two copies.

### Upstream Hub Repository

The default when the model is published on the Hub. Fetch from the original repository rather than
copying it, so that the publisher stays authoritative and a gated model is obtained under the terms
its publisher set:

```python
from huggingface_hub import hf_hub_download

MODEL_PATH = hf_hub_download(
    repo_id="amaai-lab/text2midi",
    filename="pytorch_model.bin",
    revision="<COMMIT_SHA>",
)
```

**Pin a `revision`.** Without one, the Space silently changes behavior whenever upstream moves,
which is invisible until a result changes. Prefer a commit SHA to a tag, since a tag can be moved.

Use `snapshot_download` where a model is several files that must agree with one another.

### Upstream Library

Many projects ship their own downloader, in which case the weights are a side effect of
constructing the model:

```python
from demucs import pretrained

model = pretrained.get_model("htdemucs")
```

This is the least code, and it keeps the weights consistent with the code that expects them. The
tradeoff is that the version is whatever the package pins, so pin the package itself in
`requirements.txt`. Such downloads usually happen on first use, so construct the model at module
scope to keep that cost out of the first request.

### Mirrored Model Repository

Mirror into a model repository when the weights are not on the Hub at all, such as a lab web
server, a Google Drive link, or a GitHub release asset. A model repository keeps large files out of
the Space, can be pinned by revision, and lets several Spaces share one copy.

Do not mirror weights whose license forbids redistribution. See [Licensing](#licensing).

### Storage Bucket

[Buckets](https://huggingface.co/docs/hub/storage-buckets) are S3-like object storage on the Hub,
separate from Git repositories. Use one when a weight set is large enough that retaining every
version of it is impractical, since a Git repository keeps every file it has ever held, and
repeated updates to a multi-gigabyte checkpoint grow it without bound. Our
[`teamup-tech/jukebox-weights`](https://huggingface.co/teamup-tech/buckets), at 10.3 GB, is the
existing example.

```python
from huggingface_hub import download_bucket_files

download_bucket_files(
    "teamup-tech/jukebox-weights",
    files=[("prior/model.pt", "./weights/prior.pt")],
)
```

Bucket files are also addressable as `hf://buckets/teamup-tech/<bucket>/<path>` through any
fsspec-aware library.

**Buckets are non-versioned and mutable.** There is no `revision` to pin, overwriting a file
replaces it in place, and deletions are immediate and permanent. A Space reading from a bucket
reads whatever is in it now. Treat the contents as append-only, and give each set of weights its
own prefix rather than overwriting one in place:

```
jukebox-weights/
  5b-lyrics-v1/model.pt
  5b-lyrics-v2/model.pt
```

Prefer a model repository whenever the size makes it practical, and a bucket only when it does
not.

### Committed to the Space

Reasonable only for small assets, roughly under 50 MB, with no home of their own. The asset is
then versioned with the Space itself. The Hub rejects binary files pushed directly to Git, and
`.gitattributes` has to be in place **before** the file is staged. PyHARP's README covers the
failure modes in its
[Binary Files](https://github.com/TEAMuP-dev/pyharp/blob/develop/README.md#binary-files) section.

### Persistent Storage

A paid Space feature that keeps files across rebuilds. The contents are not versioned and a
rebuild does not reset them, so the Space becomes harder to reproduce. Avoid it unless nothing
else works.

### Multiple Weight Sets

Where a Space offers a choice of checkpoints, load them lazily and cache them, so that startup does
not pay for weights nobody selects:

```python
LOADED_MODELS = {}

def get_cached_model(model_name: str):
    if model_name not in LOADED_MODELS:
        model = pretrained.get_model(model_name)
        model.eval()
        model.to(device)
        LOADED_MODELS[model_name] = model
    return LOADED_MODELS[model_name]
```

Expose the choice as a `gr.Dropdown` whose options are named the way the upstream project names
them.

## Error Reporting

**Always launch with `show_error=True`:**

```python
demo.queue().launch(share=True, show_error=True, pwa=True)
```

Gradio forwards the text of an exception only when `show_error=True` is set, or when the exception
is a `gr.Error`. Without either, HARP can report that the model failed but nothing about why.

**Raise `gr.Error` for failures a user can act on.** Its message is forwarded regardless of
`show_error`, and it reads as deliberate rather than as a crash:

```python
if signal.sample_rate != 44100:
    raise gr.Error("This model requires 44.1 kHz audio.")
```

Validate inputs at the top of `process_fn`, and say what was wrong and what is expected.

`gr.Info` and `gr.Warning` never reach HARP. Gradio does not forward them on the endpoint HARP
uses, so they appear only on the Gradio page. Do not rely on them to communicate anything.

This matters more once `process_fn` runs in a [worker](#worker-processes). A `gr.Error` crosses
back intact, while any other exception arrives as a `RuntimeError` carrying only its message. The
traceback is left in the Space logs.

## Preserving the Input Format

**Return audio the way it arrived.**

`load_audio` preserves whatever the file carried, and `save_audio` writes `.wav` at the signal's
current rate. Preserving the format therefore means capturing the input's properties before
processing and restoring them afterward.

### Sample Rate

Always restore it. Capture the rate before anything touches the signal, resample for inference if
the model requires a fixed rate, and resample back before saving:

```python
signal = load_audio(input_audio_path)

original_sr = signal.sample_rate

if signal.sample_rate != MODEL_SAMPLE_RATE:
    signal.resample(MODEL_SAMPLE_RATE)

... # inference

signal.resample(original_sr)
```

### Container

Preserve the container only when it is **lossless**. HARP accepts `.wav`, `.aiff`, `.flac`, `.ogg`,
and `.mp3`, so a Space can be handed a file that carries one lossy encode already. Processing moves
that encoder's artifacts out from under the maskers that hid them, and writing the result back as
`.mp3` or `.ogg` then adds a second round of loss over a first that has become audible. Return a
lossless format instead:

```python
from pathlib import Path

LOSSLESS = {".wav", ".aiff", ".flac"}

input_ext = Path(input_audio_path).suffix.lower()
output_ext = input_ext if input_ext in LOSSLESS else ".wav"

output_audio_path = str(save_audio(signal, get_default_path(ext=output_ext)))
```

**Leave `format` unset on the `gr.Audio` output when doing this.** Gradio converts the returned
audio to whatever `format` names, so a component declaring `format="wav"` rewrites the `.flac` the
code above selected, and the conversion is invisible from inside `process_fn`. The two are mutually
exclusive: declare `format` where the model fixes its output format, and leave it off where the
output format depends on the input. The tags follow suit, so `output:audio/wav` is a fixed format
and `output:audio` is one that varies.

### Channels

Channels cannot simply be restored, because a model that collapses stereo to mono destroys the
stereo image rather than setting it aside. Prefer these in order:

1. **Run the channels independently and recombine them.** This preserves the stereo image and is
   correct whenever the model is channel-agnostic. It costs roughly twice the inference time.
2. **Duplicate the mono result across the input's channel count.** When the model is inherently
   mono, this is the correct fallback:

   ```python
   original_channels = signal.num_channels

   signal = signal.to_mono()

   ... # inference

   if signal.num_channels != original_channels:
       signal.audio_data = signal.audio_data.repeat(1, original_channels, 1)
   ```

   The file then has the channel count the session expects. Two identical channels play back the
   same as a mono file on a stereo track.

   It does not make the output stereo. Say so in the model card description, or the duplication
   hides that the stereo image was discarded.

Never do the reverse. If the input is mono and the model genuinely produces two distinct channels,
that is new information, and collapsing it back to mono throws away the model's actual output.

### MIDI

The same principle applies. Preserve ticks per beat, tempo, and track structure unless changing
them is the point of the model.

## Hardware Portability

**A Space should run unchanged on CPU, on a dedicated GPU, and on ZeroGPU.** Switching hardware in
the Space settings should never require editing `app.py`.

```python
import spaces
import torch

device = "cuda" if torch.cuda.is_available() else "cpu"

model = MyModel.from_pretrained(...)
model.eval()
model.to(device)

@spaces.GPU(duration=120)
def process_fn(input_audio_path: str, ...) -> str:
    ...
```

Three things make this portable:

- **`@spaces.GPU` is effect-free off ZeroGPU.** The decorator does nothing on a CPU or dedicated GPU
  Space, so it can be left in place unconditionally. Add `spaces` to `requirements.txt` regardless
  of the hardware currently selected.
- **Load the model at module scope and move it to the device there.** This is counterintuitive on
  ZeroGPU, where a real GPU exists only inside the decorated function, but Hugging Face enables a
  CUDA emulation mode outside it precisely so that module-level placement works. Lazy-loading or
  calling `.to("cuda")` inside the decorated function is explicitly discouraged and is measurably
  slower, since CUDA transfers are optimized for placement during startup.
- **Select the device rather than hardcoding it.** `torch.cuda.is_available()` is what keeps the
  same line working on a CPU Space, where `.to("cuda")` would fail outright.

Select ZeroGPU (`zero-a10g`) for a model that needs a GPU, since it costs nothing, and `cpu-basic`
for one that does not.

Set `duration` to a realistic ceiling for one request. It is independent of `timeout_s` (see [Worker
Processes](#worker-processes)), and the shorter of the two bounds a job. **ZeroGPU charges the
duration a call requests, not the time it uses.** Measured across 15 calls in one allowance window,
the requested durations summed to 2115 s against 2116 s of allowance consumed, while the GPU did
roughly 143 s of work. So `duration` is the price of every call, including one that fails in
seconds, and a generous ceiling is paid again by each user on each request. One Space declared 120 s
for a request that took 15 s, which spends 105 s of somebody's daily allowance on nothing. Time a
real request and set the ceiling just above it. Shorter durations also improve queue priority for
everyone using the Space.

ZeroGPU is available only to Gradio Spaces, so a Docker Space has to pay for GPU hardware instead.
It requires PyTorch 2.8.0 or newer, which rules it out for anything pinned below that, and it does
not support `torch.compile`, so use ahead-of-time compilation instead.

Quota is charged against whoever is signed in: two minutes per day unauthenticated, five minutes
for a free account, forty for PRO. A user with no API key configured in HARP therefore gets two
minutes of GPU time per day across all ZeroGPU Spaces before requests start failing on quota.

## Dependency Conflicts

Some models cannot share an environment with current Gradio. The usual cause is a transitive pin
that cannot be satisfied alongside it, such as a package relying on the `numpy.float` alias removed
in `numpy==1.24`.

### Pin Gradio Before Splitting the Environment

A model pinned to `transformers` 4.x is the common case, and it looks irreconcilable, since
`transformers` below 5.0 caps `huggingface-hub` below 1.0 while Gradio 6 wants it above. Gradio's
floor moved over the 6.x series, so which Gradio is pinned is what decides whether the two can
coexist:

| Gradio | Requires |
| --- | --- |
| 6.13 to 6.17 | `huggingface-hub>=0.33.5` |
| 6.18 to 6.20 | `huggingface-hub>=1.2.0` |
| 6.21 and later | `huggingface-hub>=1.16.0` |

PyHARP requires Gradio 6.13 or newer for the error payload, so 6.13 through 6.17 is the window
where a `transformers` 4.x model and PyHARP resolve together, landing on `huggingface-hub` 0.36.
Pin `sdk_version` there and check the resolution before concluding that an environment has to be
split:

```bash
pip install --dry-run --ignore-installed -r requirements.txt \
    "gradio==6.17.3" "git+https://github.com/TEAMuP-dev/pyharp.git@v0.4.1"
```

A Space pinned inside that window should say why in its README, since it departs from the version
the rest of the fleet runs.

### Split the Environment

Keep the two apart rather than patching the model's source. A **frontend** environment runs Gradio
and PyHARP, a **backend** environment runs the model under its own interpreter and pins, and the
frontend invokes the backend as a subprocess, exchanging JSON. Both live in one Docker image, which
a Gradio Space cannot express, so the Space uses the Docker SDK. PyHARP's
[Docker Spaces](https://github.com/TEAMuP-dev/pyharp/blob/develop/README.md#docker-spaces) section
has the layout, and [BeatNet-dual](https://huggingface.co/spaces/teamup-tech/BeatNet-dual) is a
working example.

ZeroGPU is not available to Docker Spaces, so a model that needs a GPU has to be paid for (see
[Hardware Portability](#hardware-portability)). A model that runs on CPU is unaffected.

Two things are easy to get wrong:

- **Invoke the backend with `subprocess.run`.** The worker leads its own process group, so a
  cancellation then tears down the model along with the job (see
  [Worker Processes](#worker-processes)).
- **Write only the result to stdout.** The frontend parses it as JSON, so logging and progress have
  to go to stderr.

## Calling Another Space

A Space can forward inference to a second Space rather than running the model itself. It is the
last resort among the options here, because it makes a request depend on a deployment outside this
one, and it is warranted only where the model cannot be installed alongside Gradio and PyHARP at
all. Three reasons have held up:

- **A pin that conflicts with the platform rather than with Gradio.** Audiocraft pins
  `torch==2.1.0` and ZeroGPU requires 2.8.0 or newer, so no choice of Gradio reconciles them.
- **A vendoring surface too large to carry.** SoulX-Singer's inference path reaches 56 modules and
  34 distributions, two of them CUDA-only, which would leave the Space impossible to exercise on
  CPU.
- **No installable distribution.** ACE-Step's package on PyPI cannot be built, since its
  `setup.py` reads a `requirements.txt` that the sdist omits.

Model size alone is not a reason: weights are fetched at build time either way (see
[Model Weights](#model-weights)). Neither is a `transformers` 4.x pin on its own, which the Gradio
window above usually settles.

### Host the Backend Here

Point the frontend at a Space under `teamup-tech`, duplicated from the authors' where necessary,
rather than at the authors' own. A third-party Space can change its API, fall asleep, or be taken
down, and none of that is apparent until a request fails. Hosting the backend in the org also puts
its hardware and its allowance where they can be seen.

Keep that backend public. A private Space is reachable over its API, but only with a token that can
read it, which no user has. The frontend would then have to call with the org's own token, and
every user's request would draw on that one account rather than their own.

### Whose Allowance Is Spent

Allowance is charged to whoever authenticates the request, so for a proxy it follows the identity
the frontend presents to the backend:

| The frontend calls with | Whose allowance is spent |
| --- | --- |
| No token | The anonymous pool, two minutes a day for everyone together |
| The org's token | One account, shared by every user of the Space at once |
| The token HARP supplies | The user's own, which is the right default |

Forward the user's token and fall back to anonymous. Embedding the org's token turns its forty
minutes into a fleet-wide bottleneck, and the resulting failure reads as the model being broken.

### What the Frontend Owes HARP

- **Declare the `zerogpu` tag.** The frontend itself runs on `cpu-basic`, so its hardware says
  nothing about the allowance a request draws on, and the tag is what puts the ZeroGPU badge in
  HARP (see [Tags](#tags)).
- **Say in the description that the work happens elsewhere**, and whose allowance pays for it.
- **Check the backend's API signature at startup.** Without that, an upstream change surfaces as a
  puzzle once per request instead of as a build failure.
- **Set `quota_error_exhausts_validation_token: false`** for the Space in
  `model_validation/config.yml`. A quota error raised by the backend belongs to the downstream
  token, and the harness otherwise reads it as the validation token being spent and skips every
  remaining ZeroGPU model.

## Keeping a Space Lightweight

Bring in only what inference needs. Research repositories carry training loops, evaluation
harnesses, dataset preparation, notebooks, and configuration for experiments that were never run.
None of it belongs in a Space.

- Vendor the inference path rather than the whole repository, or install the upstream package from
  a pinned Git revision if it is packaged properly.
- **Work out that path, rather than guessing it.** Walk the imports from the entry point and keep
  the closure. Follow relative imports as well as absolute ones: a package that reaches its
  submodules with `from .module import name` looks far smaller than it is, and SoulX-Singer's
  inference path grows from 14 modules to 56 once they are counted.
- **Take the data the code opens, not only the code.** Weights are the obvious part, and the
  easily missed part is everything else a model reads at runtime: an ONNX graph for a text front
  end, a phoneme table, a vocabulary pickle. Filtering a checkout by source extension drops all of
  it, and the Space then builds cleanly and fails on its first request. Diff the file list against
  the upstream repository and account for each name left over:

  ```python
  upstream = set(api.list_repo_files(source, repo_type="space"))
  ours = set(api.list_repo_files(destination, repo_type="space"))
  print(sorted(upstream - ours))
  ```

- Keep `requirements.txt` to what is imported. Every entry adds build time, paid again on every
  rebuild, so a heavy dependency tree can leave a Space taking minutes to come back after a
  restart.
- Put `apt-get` packages in `packages.txt` rather than shelling out at startup.

A Space that starts quickly also recovers quickly after sleeping.
