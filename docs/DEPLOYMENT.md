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
  all. Where a dependency rules `6.24.0` out, hold the newest release it allows and say why in the
  README body. Ten Spaces hold `6.17.3`, the newest whose `huggingface-hub` floor admits the
  transformers 4.x their models need.
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

HARP and PyHARP are released as compatible pairs. **Pin `pyharp` to a release tag** in
`requirements.txt`, never to a branch:

```
git+https://github.com/TEAMuP-dev/pyharp.git@<TAG>
```

A branch such as `develop` moves under the Space, so a rebuild can change how it behaves with
nothing in the repository having changed. **A released tag never moves**: a fix ships as a new patch
release instead. pip caches the wheel it built for a URL, so a Space pinned to a tag that is later
moved goes on installing whatever the tag named when the Space was last built, through rebuilds and
factory reboots alike, and nothing in the installed metadata disagrees.

Should a tag ever move, repin the Spaces built against it to the commit it now names. That is a new
URL, which forces a fresh install, and naming the release in a comment above it keeps the file
readable.

Gradio is pinned separately, by `sdk_version` in the [Space README](#space-readme), and the two have
to agree. The latest PyHARP requires `gradio>=6.13.0,<7`, below which Gradio discards the payload
carrying the reason a job failed (see [Error Reporting](#error-reporting)).

So `requirements.txt` holds `pyharp` pinned to a release, `spaces` (see
[Hardware Portability](#hardware-portability)), and the model's own dependencies, kept to what the
app imports (see [Keeping a Space Lightweight](#keeping-a-space-lightweight)). It does not hold
`gradio`.

**Pin the package that implements the model** to an exact version, since `app.py` calls its API and
a rebuild otherwise installs whatever is newest.

A package that another dependency requires by a Git URL is the exception. pip refuses one package
named by two different references, even when both resolve to the same commit, so pinning it in
`requirements.txt` fails the build. Leave it as the dependency names it, with a comment saying why
it is unpinned.

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

- **Load the model in the worker alone.** Module scope runs in both processes, but only the worker
  runs `process_fn`, so the app has no use for the model. Loading it there as well holds it twice,
  and where both processes download the same file at once, one can read it before the other has
  finished writing it. Load when the module is imported as anything other than
  `__main__`, which the worker's import is:

  ```python
  if __name__ != "__main__":
      model = load_model()
  ```

  The app still binds its port at once, since it skips the load, and the worker, which starts with
  the Space (see below), finishes loading before `process_fn` can reserve a GPU. On ZeroGPU, place
  the model on CUDA here, on the worker's main thread.

  Many Spaces written before PyHARP 0.4.2 load in both processes, at module scope or in a background
  thread that the worker joins. Loading at module scope is safe, if wasteful, because the app
  finishes it before it starts the worker. A thread is safe only while everything it downloads goes
  through the Hub's cache, which locks each file. Anything else, such as a URL fetched to a fixed
  path, is still being written by the app's thread when the worker's own thread starts.
- **The worker starts as the Space comes up.** Since PyHARP 0.4.2, `build_endpoint` starts it at
  once (`warm_start`), so its import and every model it loads are done before the first request in
  most cases. A request that arrives while the worker is still loading waits for it, and on ZeroGPU
  a wait of more than about two minutes outlasts the request's token: a worker that spent 2 min 15 s
  loading reached the GPU with the token gone, and the request failed with "Expired ZeroGPU proxy
  token".
- **`build_endpoint` takes `timeout_s`**, defaulting to 900 seconds. Raise it for a model that
  legitimately runs longer.
- **A Space that shells out should use `subprocess.run`.** The worker leads its own process group,
  so a cancellation then tears down what the job started instead of orphaning it.
- **A process pool that `process_fn` starts uses spawn.** The worker was itself spawned, which makes
  spawn its default start method, so each process of a `multiprocessing` pool imports `app.py` and
  the library afresh. A patch that another package applies on import, such as a compatibility shim
  for an older library, never reaches them. Fix such a library in its own files at build time, so
  that it imports on its own.

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

**Label each output with what it holds.** HARP tells results apart by their file names, which
matters most in its media clipboard, where results from many runs and models sit side by side. A
name like `tmp3kx9q.wav` or `output.wav` is soon lost among them. PyHARP renames every file
`process_fn` returns as `<model>_<output>`, from the model card's name and the output's label (see
[Output File Names](https://github.com/TEAMuP-dev/pyharp/blob/develop/README.md#output-file-names)).
The label is the part a Space chooses, so make it say what the result is: "Vocals", "Accompaniment",
or "Restored Audio", never "Output Audio" or "Result". Return each result as a file rather than as
an array, and a `gr.Audio` result in the `format` it declares, since Gradio names any audio it has
to write itself `audio.wav`.

**Return labels as a `LabelList`, over a track.** HARP reads every `gr.JSON` output as a PyHARP
`LabelList` and fails the whole request on anything else, so a plain dictionary of results shows
the user an error after the model has done its work. It also draws labels only over an audio, MIDI
or file output the model returns. An analysis model therefore returns its input as an audio output
alongside the labels, as Beat-This and S-KEY do, and a whole-clip result such as a tag, a score or
a loudness reading becomes a label spanning the clip.

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
[worker](#worker-processes) is warm before the next request. On ZeroGPU a fetch inside `process_fn`
also runs inside the GPU reservation, which a large download can outlast, and it can fail outright.

Load the weights in the worker alone, as [Worker Processes](#worker-processes) describes. A Space
that loads them at module scope in both the app and the worker holds the model twice: host memory
in either case, and device memory as well on a dedicated GPU, where `.to(device)` also runs in both.
Size such a Space's hardware for two copies.

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

A library that downloads from outside the Hub also ties the Space to that server.
The fleet reads the same file from a
[mirror](#mirrored-model-repository) instead, linked where the library looks before it downloads:

```python
checkpoint = Path(torch.hub.get_dir()) / "checkpoints" / "beat_this-final0.ckpt"

if not checkpoint.exists():
    checkpoint.parent.mkdir(parents=True, exist_ok=True)
    checkpoint.symlink_to(hf_hub_download("teamup-tech/Beat-This-weights", "final0.ckpt",
                                          revision="<COMMIT_SHA>"))
```

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

Where a Space offers a choice of checkpoints, fetch every one when the worker starts, so that no
request waits on a download, which on ZeroGPU would run inside its GPU reservation. Building them is
a separate choice. Build them all at startup when they fit in memory together, which keeps every
request fast and, on ZeroGPU, is the only way a build is not repeated on every call. Otherwise build
each the first time it is selected and cache it:

```python
REVISIONS = {"small": "<COMMIT_SHA>", "large": "<COMMIT_SHA>"}
WEIGHTS = {}
LOADED_MODELS = {}

if __name__ != "__main__":
    for size, revision in REVISIONS.items():
        WEIGHTS[size] = hf_hub_download(f"owner/model-{size}", "model.safetensors",
                                        revision=revision)

def get_cached_model(size: str):
    if size not in LOADED_MODELS:
        LOADED_MODELS[size] = load_model(WEIGHTS[size]).eval().to(device)
    return LOADED_MODELS[size]
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
if signal.duration < 3:
    raise gr.Error(f"Key detection needs at least 3 seconds of audio, but received {signal.duration:.1f}.")
```

Validate inputs at the top of `process_fn`, and say what was wrong and what is expected. Refuse
only what the model cannot use. A sample rate or channel count it does not run at can be converted
instead (see [Preserving the Input Format](#preserving-the-input-format)).

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
def separate(input_audio_path: str, ...) -> str:
    ...
```

Three things make this portable:

- **`@spaces.GPU` is effect-free off ZeroGPU.** The decorator does nothing on a CPU or dedicated GPU
  Space, so it can be left in place unconditionally. It goes on the function that does the model's
  work, which `process_fn` calls once its checks pass (see [Protecting the Caller's
  Allowance](#protecting-the-callers-allowance)). Add `spaces` to `requirements.txt` regardless of
  the hardware currently selected.
- **Load the model at module scope and move it to the device there.** This is counterintuitive on
  ZeroGPU, where a real GPU exists only inside the decorated function, but Hugging Face enables a
  CUDA emulation mode outside it precisely so that module-level placement works. Lazy-loading or
  calling `.to("cuda")` inside the decorated function is explicitly discouraged and is measurably
  slower, since CUDA transfers are optimized for placement during startup. ZeroGPU runs each call
  in a fresh fork, so a model built inside the function is built again on every call. The one
  exception found so far is a model holding a sparse tensor, which the emulation cannot register
  ("Cannot access storage of SparseTensorImpl").
- **Select the device rather than hardcoding it.** `torch.cuda.is_available()` is what keeps the
  same line working on a CPU Space, where `.to("cuda")` would fail outright.

A Space with no model, such as one that only runs signal processing, is the exception. A GPU does
nothing for it, so it belongs on CPU (see [Choosing the Hardware](#choosing-the-hardware)), and a
decorated function would only have ZeroGPU bill its callers for CPU work. It leaves the decorator
out.

### Choosing the Hardware

**Put a model on ZeroGPU only when the GPU changes what using it is like.** ZeroGPU is free for the
organization but not for its users: every call spends the caller's daily allowance, can be refused
when that runs short, and pays a cold start whenever a worker is spawned. A call to a `cpu-basic`
Space costs the user nothing. An organization can also run at most **50 Spaces on ZeroGPU**.

Decide by measurement. Time a warm request with 10 seconds of input on `cpu-basic`, which the
validation harness reports per case (the first case on a fresh worker also pays for loading the
model, so read the one after it):

- **Faster than real time, under about 10 s for 10 s of input: keep it on CPU.** Analysis models
  that make one pass over the audio (key, tempo, emotion, tagging, detection), small effect models,
  and light speech synthesis such as Kokoro measured 1 to 7 s.
- **Slower than real time: ZeroGPU,** since a clip of a minute or more would otherwise take minutes.
  Diffusion and autoregressive generators, source separators, and large transcription models
  measured 11 to 300 s.
- **TensorFlow and other non-PyTorch models stay on CPU,** since ZeroGPU accelerates PyTorch only,
  however slow.

On ZeroGPU, `spaces` has to be imported before anything that initializes CUDA, PyHARP included,
since importing it loads torch. Otherwise the Space fails at startup with "CUDA has been initialized
before importing the `spaces` package", which is why the example above imports it first.

ZeroGPU requires PyTorch 2.8.0 or newer, and an exact `torch` pin must be a release it accepts,
which at the time of writing are 2.8.0, 2.9.1, 2.10.0, 2.11.0, 2.12.1, and 2.13.0. Any other exact
pin, _e.g._ 2.9.0, stops the Space with a configuration error before it starts, while a lower
bound or no pin passes. It does not support `torch.compile`, so use ahead-of-time compilation
instead.

ZeroGPU is available only to Gradio Spaces, so a Docker Space has to pay for GPU hardware instead,
or move to the Gradio SDK.

### Protecting the Caller's Allowance

Every ZeroGPU call spends the caller's daily allowance: two minutes unauthenticated, five for a free
account, forty for PRO, shared across every ZeroGPU Space. Avoid spending it for a mistaken setting or an input the model cannot use.

**Check inputs and settings before the GPU is reserved.** Put `@spaces.GPU` on a function that does
only the GPU work, and have an undecorated `process_fn` check everything first and then call it. A
request refused there costs nothing, waits in no GPU queue, and reaches a user with little allowance
left as the real problem rather than as a quota error. Reading a file needs no GPU, so checks of an
input's length, channels, or contents belong there too, as does waiting for a model that is still
loading. Read the file with `soundfile` there rather than `torchaudio`, whose ffmpeg decoding fails
outside the decorated function under ZeroGPU's emulation ("Creating a new Tensor subclass
ChunkTensor"):

```python
@spaces.GPU(duration=60)
def separate(audio_path: str, model_name: str) -> str:
    ...

def process_fn(audio_path: str, model_name: str) -> str:
    try:
        sf.info(audio_path)
    except Exception as exc:
        raise gr.Error(f"Could not read the audio file: {exc}") from exc

    return separate(audio_path, model_name)
```

**Refuse settings that cannot give a useful result,** not only invalid ones.

**Refuse what cannot finish, and nothing more.** Two things stop a request whatever the caller can
afford: an input beyond what the model can hold, such as its context window or the GPU's memory,
and a reservation longer than PyHARP's `timeout_s` (see [Worker Processes](#worker-processes)),
which stops the job anyway. Refuse those before the GPU is reserved. Do not cap an input so that a
free account could afford it. ZeroGPU already refuses a request that the caller's tier or remaining
allowance cannot cover, before reserving anything, with a message saying how to get more, and a PRO
account can run tasks of up to 40 minutes.

A model that holds its whole input on the GPU at once has a memory limit, which only measurement
finds, and which can sit just past a length that ran in seconds. Process a long input in
overlapping chunks, cross-faded where they meet, where the model allows. Otherwise refuse inputs
longer than the longest measured to fit.

**Size each reservation to the request.** A call that completes is billed roughly the time it used,
but one that runs past its reservation is stopped and can cost the whole reservation, which is why a
failing Space drains allowance far faster than a working one. Time real requests, the longest an
input allows among them, and compute `duration` from the arguments, padded a little above what was
measured. `@spaces.GPU` takes a function of the same arguments as the function it decorates, so a
short clip reserves, and puts at risk, only what it needs:

```python
# PyHARP stops a job that runs past its timeout_s, 15 minutes by default
MAX_GPU_SECONDS = 900

def gpu_seconds(audio_path: str, *_) -> float:
    return 10 + 0.5 * sf.info(audio_path).duration

@spaces.GPU(duration=gpu_seconds)
def separate(audio_path: str, model_name: str) -> str:
    ...

def process_fn(audio_path: str, model_name: str) -> str:
    if gpu_seconds(audio_path) > MAX_GPU_SECONDS:
        raise gr.Error("The recording is too long for one request to finish. Trim it.")

    return separate(audio_path, model_name)
```

The `spaces` client multiplies `duration` by a factor for the GPU a call lands on, at the time of
writing 1.5 on the RTX PRO 6000 Blackwell nodes and 1 on H200, and ZeroGPU admits the call only when
the caller's remaining allowance covers the result. Keep reservations at 120 s or less where the work allows, since shorter
durations also improve queue priority for everyone using the Space.

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
    "gradio==6.17.3" "git+https://github.com/TEAMuP-dev/pyharp.git@v0.4.2"
```

A Space pinned inside that window should say why in its README, since it departs from the version
the rest of the fleet runs.

### Split the Environment

Keep the two apart rather than patching the model's source. A **frontend** environment runs Gradio
and PyHARP, a **backend** environment runs the model under its own interpreter and pins, and the
frontend invokes the backend as a subprocess, exchanging JSON. Both live in one Docker image, which
a Gradio Space cannot express, so the Space uses the Docker SDK. PyHARP's
[Docker Spaces](https://github.com/TEAMuP-dev/pyharp/blob/develop/README.md#docker-spaces) section
has the layout, and [BeatNet](https://huggingface.co/spaces/teamup-tech/BeatNet) is a working
example.

ZeroGPU is not available to Docker Spaces, so a model that needs a GPU has to be paid for (see
[Hardware Portability](#hardware-portability)). A model that runs on CPU is unaffected.

Two things are easy to get wrong:

- **Invoke the backend with `subprocess.run`.** The worker leads its own process group, so a
  cancellation then tears down the model along with the job (see
  [Worker Processes](#worker-processes)).
- **Write only the result to stdout.** The frontend parses it as JSON, so logging and progress have
  to go to stderr.

## Calling Another Space

A Space can forward inference to another Space rather than running the model itself. The fleet runs
one, SoulX-Singer, and no other should be built for a model that can be installed here. Every other
proxy the fleet once ran turned out to be avoidable once these were checked:

- **Check what the authors' own Space installs.** A Space that already runs on ZeroGPU has solved
  the platform's constraints, often by vendoring a fork. MelodyFlow's carries a copy of audiocraft
  moved from `torch==2.1.0` to 2.8.0, which dissolves the conflict with ZeroGPU that the published
  package has. ACE-Step's carries its source rather than the PyPI package, whose sdist cannot be
  built.
- **Vendor that Space's code rather than calling it.** Copy what the inference path imports, along
  with the data files it opens (see [Keeping a Space Lightweight](#keeping-a-space-lightweight)),
  keep the authors' license file beside it, and call the function their interface calls.
- **Dry-run the combined environment** with the recipe under [Pin Gradio Before Splitting the
  Environment](#pin-gradio-before-splitting-the-environment) before concluding that it cannot
  resolve. Model size is not a reason either, since weights are fetched at build time whatever the
  deployment.

Calling another Space is the right deployment only where the model cannot be hosted here at all:

- **Its code or weights cannot be redistributed,** and their owners serve the model themselves, so
  calling that deployment is the only lawful route to it.
- **It needs hardware ZeroGPU cannot provide,** such as more GPU memory or a longer job than ZeroGPU
  allows, and its owners run that hardware.
- **Running it here has been tried and failed, while its owners run it.** SoulX-Singer is an example of this case.
  A reproduction gave poor output, and the authors' code, vendored once its dependencies resolved,
  loaded but hung in ZeroGPU's setup inside PyHARP's worker on every call.

In each case the backend belongs to its owners, who can change its API, let it sleep, or take it
down without notice. A backend is never hosted under `teamup-tech`, both because a model that could
be hosted here should be vendored instead and because HARP's Home tab lists every public Space in
the organization, so a backend there would appear as a model that fails to load.

### What a Frontend Owes HARP

SoulX-Singer's `app.py` does each of these, so start a new frontend from a copy of it.


- **Forward the caller's `x-ip-token` header.** The Hub adds it to every request a Space receives,
  identifying whoever made it, and ZeroGPU bills the allowance of whoever it identifies. Passed on
  to the backend, it bills each user their own allowance, as a direct call would, and the frontend
  never handles a credential. PyHARP restores the request inside its worker, so `process_fn` can
  read the header from there:

  ```python
  from gradio.context import LocalContext
  from gradio_client import Client

  def process_fn(audio_path):
      request = LocalContext.request.get(None)
      ip_token = request.headers.get("x-ip-token", "") if request else ""
      client = Client("https://owner-backend.hf.space", headers={"x-ip-token": ip_token})
      ...
  ```

  Without it, every call reaches the backend from the frontend's server, and all users share one
  anonymous allowance. Embedding a token of the organization's instead bills every user to that one
  account. Either way, once that single allowance runs out, the model looks broken to everyone.
  Forwarding also leaves a quota error from the backend attributable to the caller, which is what
  the validation harness assumes of every Space. See [Calling Spaces from Another
  Space](https://huggingface.co/docs/hub/spaces-api-endpoints#calling-spaces-from-another-space).
- **Reach the backend by its URL,** `https://<owner>-<name>.hf.space`, as above, not by its
  repository id. Given an id, `gradio_client` looks the host up through the Hub's API without a
  token, which the Hub rate-limits from the address a Space shares with others.
- **Declare the `zerogpu` tag** when the backend runs on ZeroGPU. The frontend itself runs on
  `cpu-basic`, so its hardware says nothing about the allowance a request draws on, and the tag is
  what puts the ZeroGPU badge in HARP (see [Tags](#tags)).
- **Say in the description where the work happens** and whose allowance pays for it.
- **Check the backend's API signature at startup,** so that an upstream change surfaces as a build
  failure rather than as a puzzle on every request.

## Keeping a Space Lightweight

Bring in only what inference needs. Research repositories carry training loops, evaluation
harnesses, dataset preparation, notebooks, and configuration for experiments that were never run.
None of it belongs in a Space.

- Vendor the inference path rather than the whole repository, or install the upstream package from
  a pinned Git revision if it is packaged properly.
- **Work out that path, rather than guessing it.** Walk the imports from the entry point and keep
  the closure. Follow relative imports as well as absolute ones: a package that reaches its
  submodules with `from .module import name` looks far smaller than it is.
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
