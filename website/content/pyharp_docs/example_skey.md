# Example 1: Key Detection

This example walks through the app behind our
[S-KEY](https://huggingface.co/spaces/teamup-tech/S-KEY) model, which finds the key of a piece of
music and marks it over the audio in HARP. The model is small enough to run on any CPU in seconds,
which leaves the steps every deployment shares in plain view: describing the model, loading it once,
processing a request, and declaring the interface.

The code shown is the deployed app, at revision
[`6a3c10c`](https://huggingface.co/spaces/teamup-tech/S-KEY/tree/6a3c10c417884132e60a12cf3625de0dfe86e5e2).

## 1. Get the Code

Download the Space at that revision into a folder of its own, and install its requirements in an
environment there:

```bash
mkdir s-key && cd s-key
python3 -m venv .venv
source .venv/bin/activate        # On Windows: .venv\Scripts\activate
pip install huggingface_hub
hf download teamup-tech/S-KEY --repo-type space --local-dir . \
    --revision 6a3c10c417884132e60a12cf3625de0dfe86e5e2
pip install -r requirements.txt
```

The Space holds:

- `app.py`, the PyHARP app.
- `requirements.txt`, the packages the app needs, each pinned to one version except `spaces`, which
  sets a minimum.
- `skey/`, the inference code of the authors and its checkpoint of 765 KB, under their MIT
  license (`LICENSE`).
- `README.md`, the configuration of the Space (see [Hosting Models](/content/hosting/spaces)).

Copying the code that inference needs, rather than a whole research repository, keeps a Space small
and quick to start, as
[Lightweight Spaces](https://github.com/TEAMuP-dev/HARP/blob/main/docs/DEPLOYMENT.md#lightweight-spaces)
describes.

## 2. Describe the Model

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
    tags=[Subcategory.MUSIC_ANALYSIS, SampleRate(22050), Channels(1)],
)
```

- **`spaces` comes first.** On ZeroGPU it must be imported before anything that loads PyTorch, and
  elsewhere it does nothing, so the same file runs on any hardware.
- **The model card** names the model, credits its authors, says what it does, links the code and
  the paper, and places it in the taxonomy. HARP shows the card at the top of the model tab, and
  lists the model under `Analysis` on the Home tab. `SampleRate` and `Channels` declare the format
  in which the model works, which nothing can infer, and the `input:` and `output:` tags are added
  for you, from the components in step 5.

## 3. Load the Model Once

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

PyHARP runs `process_fn` in a worker process, which imports `app.py` under the name `__mp_main__`.
This condition loads the checkpoint and builds the model in the worker alone, once, as the app
starts. The server process, which never runs `process_fn`, skips it.

## 4. Process a Request

```python
# Define the process function
def process_fn(input_audio_path):
    # Checked before the GPU is reserved, so a request the model cannot take spends none of the
    # caller's ZeroGPU allowance
    if not input_audio_path:
        raise gr.Error("Provide the music whose key to find.")
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
  on, which HARP shows in its error popup.
- **`find_key` runs the model.** `@spaces.GPU` reserves a GPU for it on ZeroGPU, after the checks in
  `process_fn` have passed, and does nothing elsewhere.
- **The result is a label.** A `LabelList` holds one `AudioLabel`, which spans the clip (`t` and
  `duration`), sits near the top of the track (`amplitude`), and carries its text, a description
  shown on hover, and a color. HARP draws labels over an output track, so the app returns the input
  audio, over which the label is drawn.

## 5. Declare the Interface

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


    demo.queue().launch(pwa=True, show_error=True)
```

- **`if __name__ == "__main__":`** keeps the worker, which imports `app.py`, from building the
  interface a second time.
- **The components follow `process_fn`.** The inputs match its arguments in order, and the outputs
  match its return values: an audio track, and a `gr.JSON` that HARP reads as labels.
- **`type="filepath"`** is required of every `gr.Audio`. `.harp_required(True)` marks an input that
  HARP must fill, and `.set_info(...)` describes a component in the instructions panel of HARP.
- **`demo.queue()`** lets HARP cancel a job, and **`show_error=True`** forwards the reason a job
  failed.

## 6. Run It in HARP

```bash
python app.py
```

Open `http://localhost:7860` in HARP as a custom path, as in
[Setting Up PyHARP](/content/pyharp_docs/setup#open-it-in-harp). Drag a song of at least three
seconds onto `Input Audio`, and select `Process`. The key appears as a label over `Audio with Key`.

<!-- TODO: screenshot. S-KEY open in HARP after processing a song, with the detected key drawn as a
     label over the output track.
![The key of a song, drawn as a label by S-KEY in HARP](/images/skey-in-harp.png)
-->

To deploy a model of your own from this app, replace the `skey` package and the body of `find_key`
with your own inference code, and change the model card and components to match.

**Next:** [Example 2: Stem Separation](/content/pyharp_docs/example_demucs).
