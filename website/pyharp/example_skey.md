---
description: Build a PyHARP app from scratch around S-KEY, which detects the key of a song and shows it as a label over the audio in HARP.
---

# Example 1: Key Detection

This example builds a PyHARP app from scratch around [S-KEY](https://github.com/deezer/skey), a
model that finds the key of a piece of music, and marks that key over the audio in HARP. The model
is small enough to run on any CPU in seconds, which leaves the steps every deployment shares in
plain view: describing the model, loading it once, processing a request, and declaring the
interface.

The app you end up with is the one behind our
[S-KEY](https://huggingface.co/spaces/teamup-tech/S-KEY) model. Each block of code below is the
next part of its `app.py`, as deployed at revision
[`10274b3`](https://huggingface.co/spaces/teamup-tech/S-KEY/tree/10274b3ecff836b60521c5cc4d53fb9501df898f),
so you can compare your file with ours at any point.

::: tip
This app follows our [deployment guidelines](/pyharp/guidelines), against which we check every
model we host. The steps below link the sections that apply.
:::

## 1. Set Up the Project

Create a folder for the app, with a Python environment of its own:

```bash
mkdir s-key && cd s-key
python3 -m venv .venv
source .venv/bin/activate        # On Windows: .venv\Scripts\activate
```

S-KEY is not on PyPI, so the app carries a copy of the code its authors published, and calls that
code rather than reimplementing it. Clone their repository at the commit this example uses, and
copy the `skey` package and its license into the folder:

```bash
git clone https://github.com/deezer/skey.git upstream
git -C upstream checkout 80d9590e150b836a341dfc6dbf05aa20bc81d10b
cp -r upstream/skey upstream/LICENSE .
```

The `skey` package holds the inference code and a checkpoint of 765 KB, under the MIT license.
Copying what inference needs, rather than a whole research repository, keeps a Space small and
quick to start, as [Lightweight Spaces](/pyharp/guidelines#lightweight-spaces) describes.

Next, list what the app installs in a file named `requirements.txt`:

```
torch==2.13.0
torchaudio==2.11.0
torchcodec==0.17.0
einops==0.8.2
nnAudio==0.3.4
git+https://github.com/TEAMuP-dev/pyharp.git@v0.4.2
spaces>=0.51.1
```

Each package is pinned to one version, and PyHARP to a release tag, so that the app keeps
installing what it was tested with (see
[Versions and Compatibility](/pyharp/guidelines#versions-and-compatibility)). The exception is
`spaces`, the package through which an app uses ZeroGPU, which sets a minimum. Install the list:

```bash
pip install -r requirements.txt
```

Last, create an empty file named `app.py` beside `requirements.txt`. Each step below adds the next
part of it.

## 2. Describe the Model

Begin `app.py` with its imports and the model card:

```python
import spaces
import gradio as gr
import soundfile as sf
import torch
from pyharp import ModelCard, build_endpoint, LabelList, AudioLabel, OutputLabel
from pyharp.tags import Channels, SampleRate, Subcategory
from skey.key_detection import infer_key, load_audio, load_checkpoint, load_model_components

# Check for GPU availability
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"

# Create a ModelCard
model_card = ModelCard(
    name="S-KEY",
    description=(
        "Major/Minor Key detection from audio using Self-supervised Key "
        "Detection. The input is mixed to mono and resampled to 22.05 kHz internally, and "
        "needs at least 3 seconds. Repository: https://github.com/deezer/skey Paper: "
        "https://arxiv.org/abs/2501.12907"
    ),
    author="Yuexuan Kong, Gabriel Meseguer-Brocal, Vincent Lostanlen, et al.",
    tags=[Subcategory.MUSIC_ANALYSIS, SampleRate(22050), Channels(1), 'domain:music'],
)
```

- **`spaces` comes first.** On ZeroGPU it must be imported before anything that loads PyTorch, and
  elsewhere it does nothing, so the same file runs on any hardware.
- **The model card** names the model, credits its authors, says what it does, links the code and
  the paper, and places it in the taxonomy. HARP shows the card at the top of the model tab, and
  lists the model under `Analysis` on the Home tab. `SampleRate` and `Channels` declare the format
  in which the model works, which nothing can infer, `'domain:music'` is a custom tag naming the
  domain of the model, and the `input:` and `output:` tags are added for you, from the components
  in step 5. [Model Card](/pyharp/guidelines#model-card) covers what each field should say.

## 3. Load the Model Once

Next, load the model:

```python
# Key detection fails on less audio than this (2.9 s fails, 3 s works)
MIN_INPUT_SECONDS = 3.0

# pyharp runs process_fn in a worker, which imports this file as __mp_main__ and starts with the
# server, so the checkpoint is loaded and the model built and placed there, once, rather than by
# every request as the authors' detect_key does
if __name__ != "__main__":
    CHECKPOINT = load_checkpoint()
    HCQT, CHROMANET, CROP_FN = load_model_components(CHECKPOINT, torch.device(DEVICE))
```

PyHARP runs `process_fn` in a worker process, so that the `Cancel` button in HARP can stop a job.
The worker imports `app.py` under the name `__mp_main__`. This condition loads the checkpoint and
builds the model in the worker alone, once, as the app starts. The server process, which never runs
`process_fn`, skips it. [Worker Processes](/pyharp/guidelines#worker-processes) covers what the
worker changes for an app.

## 4. Process a Request

Add the function that handles a request, and the one that runs the model for it:

```python
# Define the process function
def process_fn(input_audio_path):
    # Checked before the GPU is reserved, so a request the model cannot take spends none of the
    # caller's ZeroGPU allowance
    if not input_audio_path:
        raise gr.Error("Provide the music to analyze.")
    try:
        duration = sf.info(input_audio_path).duration
    except Exception as exc:
        raise gr.Error(f"Could not read the audio file: {exc}") from exc
    if duration < MIN_INPUT_SECONDS:
        raise gr.Error(
            f"The recording is {duration:.1f} s long. Key detection needs at least "
            f"{MIN_INPUT_SECONDS:.0f} seconds."
        )

    detected_key = find_key(input_audio_path)

    labels = LabelList()
    labels.labels.append(
        AudioLabel(
            t=0.0,
            duration=duration,
            label=detected_key,
            amplitude=0.8,
            description="Detected by S-KEY",
            color=OutputLabel.rgb_color_to_int(255, 100, 100, 0.8),
        )
    )

    return input_audio_path, labels


@spaces.GPU
def find_key(input_audio_path):
    audio = load_audio(input_audio_path, CHECKPOINT["audio"]["sr"]).to(DEVICE)
    key = infer_key(HCQT, CHROMANET, CROP_FN, audio, torch.device(DEVICE))

    # infer_key returns "error" rather than raising when inference fails
    if key == "error":
        raise gr.Error("Key detection failed on this recording.")

    return key
```

- **`process_fn` checks the input first**, and raises `gr.Error` with a message the user can act
  on, which HARP shows in its error popup (see
  [Error Reporting](/pyharp/guidelines#error-reporting)).
- **`find_key` runs the model.** `@spaces.GPU` reserves a GPU for it on ZeroGPU, after the checks in
  `process_fn` have passed, and does nothing elsewhere.
- **The result is a label.** A `LabelList` holds one `AudioLabel`, which spans the clip (`t` and
  `duration`), sits near the top of the track (`amplitude`), and carries its text, a description
  shown on hover, and a color. HARP shows labels over an output track, so the app returns the input
  audio as the track that carries the label.

## 5. Declare the Interface

End `app.py` with the components and the line that starts the app:

```python
# Build Gradio Endpoint
if __name__ == "__main__":
    with gr.Blocks() as demo:
        input_audio = gr.Audio(label="Input Audio", type="filepath").harp_required(True).set_info((
            "Music to analyze, at least 3 seconds long. The detected key comes back "
            "as a label over the audio."
        ))
        output_audio = gr.Audio(label="Audio with Key", type="filepath").set_info((
            "The input, returned to carry the key label."
        ))
        output_labels = gr.JSON(label="Analysis Labels").set_info(
            "The detected key, as a label spanning the clip."
        )

        build_endpoint(
            model_card=model_card,
            process_fn=process_fn,
            input_components=[input_audio],
            output_components=[output_audio, output_labels],
        )


    demo.queue().launch(pwa=True, show_error=True, ssr_mode=False)
```

- **`if __name__ == "__main__":`** keeps the worker, which imports `app.py`, from building the
  interface a second time.
- **The components follow `process_fn`.** The inputs match its arguments in order, and the outputs
  match its return values: an audio track, and a `gr.JSON` that HARP reads as labels.
- **`type="filepath"`** is required of every `gr.Audio`. `.harp_required(True)` marks an input that
  HARP must fill, and `.set_info(...)` describes a component in HARP's instructions panel.
- **`demo.queue()`** lets HARP cancel a job, and **`show_error=True`** forwards the reason a job
  failed.
- **`ssr_mode=False`** turns off the server-side rendering of the Gradio page, which HARP does not
  use, and which can crash a Space as it starts.
  [Structure of app.py](/pyharp/guidelines#structure-of-apppy) lists everything HARP requires of
  the components and of this line.

## 6. Run It in HARP

The app is complete. Start it from its folder:

```bash
python app.py
```

Open `http://localhost:7860` in HARP as a custom path, as in
[Setting Up PyHARP](/pyharp/setup#open-it-in-harp). Drag a song of at least three
seconds onto `Input Audio`, and select `Process`. The key appears as a label over `Audio with Key`.

![The key of a song, shown as a label by S-KEY in HARP](/images/skey-in-harp.png)

A model of your own takes the same steps: your inference code in place of the `skey` package and
of the body of `find_key`, and a model card and components to match.

**Next:** [Example 2: Stem Separation](/pyharp/example_demucs).
