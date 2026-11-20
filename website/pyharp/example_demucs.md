---
description: Build a PyHARP app from scratch around Demucs, which separates a song into stems on a ZeroGPU Space.
---

# Example 2: Stem Separation

This example builds a second app from scratch, around
[Demucs](https://github.com/facebookresearch/demucs), which separates a song into drums, bass,
vocals, and other. It adds what larger models need to [Example 1](/pyharp/example_skey): a GPU
reserved for each request in proportion to its length, weights read from a mirror on the Hub, a
choice among several models built as the app starts, and results in the format of the input.

The app you end up with is the one behind our
[Demucs](https://huggingface.co/spaces/teamup-tech/Demucs) model, which
[Using Hosted Models](/usage/models) runs in HARP. Each block of code below is the next part of its
`app.py`, as deployed at revision
[`cf386f1`](https://huggingface.co/spaces/teamup-tech/Demucs/tree/cf386f15ccde5e0fd7dc332b8e6509d3f22d7ec8),
so you can compare your file with ours at any point.

::: tip
This app follows our [deployment guidelines](/pyharp/guidelines), against which we check every
model we host. The steps below link the sections that apply.
:::

## 1. Set Up the Project

Create a folder for the app, with a Python environment of its own. Use Python 3.10 or 3.12, the two
versions that ZeroGPU runs:

```bash
mkdir demucs && cd demucs
python3 -m venv .venv
source .venv/bin/activate        # On Windows: .venv\Scripts\activate
```

Demucs is on PyPI, so the app installs it like any other package. List what the app installs in a
file named `requirements.txt`:

```
git+https://github.com/TEAMuP-dev/pyharp.git@v0.4.2

# Demucs & dependencies
demucs==4.0.0
dora-search==0.1.11
einops==0.6.1
julius==0.2.8
lameenc==1.9.0
openunmix==1.2.1

# Audio & ML
torch==2.8.0
torchaudio==2.8.0
soundfile==0.14.0
numpy==1.26.4
spaces>=0.51.1
```

The list pins PyHARP to a release tag, `demucs` and its dependencies to exact versions, and PyTorch
to a release that ZeroGPU supports. It also names `spaces`, the package through which an app uses
ZeroGPU. Install the list:

```bash
pip install -r requirements.txt
```

Demucs also runs on a CPU, more slowly, so the app works on a machine without a GPU. Last, create
an empty file named `app.py` beside `requirements.txt`. Each step below adds the next part of it.

## 2. Describe the Model

Begin `app.py` with its imports and the model card:

```python
import spaces
from pyharp.core import ModelCard, build_endpoint
from pyharp.media.audio import save_audio
from pyharp.tags import Channels, SampleRate, Subcategory

from audiotools import AudioSignal
from demucs import pretrained
from demucs.apply import apply_model
from huggingface_hub import snapshot_download

import contextlib
import gradio as gr
import soundfile as sf
import tempfile
import torch
from pathlib import Path

# ModelCard
model_card = ModelCard(
    name="Demucs",
    description=(
        "Separates a full music track into four stems: drums, bass, vocals, and "
        "everything else. Three models are available. htdemucs, the Hybrid Transformer "
        "Demucs, gives the best overall fidelity. mdx_extra is faster with a small drop in "
        "quality, and mdx_extra_q is mdx_extra quantized, the lightest of the three, with a "
        "noticeable loss of quality. Separation runs at 44.1 kHz in stereo, and the stems "
        "come back at the rate and channel count of the input. Repository: "
        "https://github.com/facebookresearch/demucs Paper: "
        "https://arxiv.org/abs/2111.03600"
    ),
    author="Simon Rouard, Francisco Massa, Alexandre Défossez",
    tags=[Subcategory.STEM_SEPARATION, SampleRate(44100), Channels(2), 'stems', 'domain:music']
)
```

- **`spaces` is the first line** because a ZeroGPU Space has no GPU of its own. One is attached for
  the length of each call to a function decorated with `@spaces.GPU`, and between calls `spaces`
  stands in for CUDA: `torch.cuda.is_available()` returns `True`, and a model moved to `"cuda"` is
  placed on the real GPU once a call begins. For this to work, `spaces` has to be imported before
  anything that loads PyTorch.
- **The model card** follows Example 1. Its description also says what each choice of model trades,
  and that the stems come back in the format of the input.

## 3. Build the Models at Startup

Add the constants, then load the models:

```python
DEMUX_MODELS = ["htdemucs", "mdx_extra", "mdx_extra_q"]
STEM_NAMES = ["Drums", "Bass", "Vocals", "Other"]
LOSSLESS = {".wav", ".flac"}
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"

# Every model is built and placed in the worker, which runs process_fn and starts as the server
# comes up, so that no GPU call builds one. demucs would download the checkpoints from Meta's own
# server, so they are read from a pinned mirror on the Hub, linked where torch.hub looks first.
LOADED_MODELS = {}

if __name__ != "__main__":
    _weights = Path(snapshot_download(
        "teamup-tech/Demucs-weights", revision="e12e76af128e0935a0ad1734cb97974e5d27663d"
    ))
    _hub = Path(torch.hub.get_dir()) / "checkpoints"
    _hub.mkdir(parents=True, exist_ok=True)
    for _checkpoint in _weights.glob("*/*.th"):
        with contextlib.suppress(FileExistsError):
            (_hub / _checkpoint.name).symlink_to(_checkpoint)

    for _model_name in DEMUX_MODELS:
        LOADED_MODELS[_model_name] = pretrained.get_model(_model_name).eval().to(DEVICE)
```

The `demucs` package downloads the weights of a variant from a server that its authors run, the
first time it is loaded. The app reads them instead from a mirror on the Hub, pinned to one
revision, and links each checkpoint where `torch.hub` looks before downloading. It then builds and
places all three variants in the worker as the app starts, so that no request waits on a download or
a build, which on ZeroGPU would run inside its GPU reservation. `DEVICE` is `"cuda"` there, where
`spaces` answers for a GPU that is not yet attached, and `"cpu"` on a machine without a GPU, so the
same lines run on any hardware. The three fit in memory together. A family of larger models would
build its default here, and each other model the first time a request selects it.
[Model Weights](/pyharp/guidelines#model-weights) lists the other places from which an app can
read its weights.

## 4. Separate on a Reserved GPU

Add the function that runs the model, with the GPU time it reserves:

```python
# The mixture is separated in overlapping segments, so the cost grows with its length
# PyHARP stops a job that runs past its timeout_s, 15 minutes by default, so a request whose
# reservation would exceed that is refused rather than started. A shorter one is left to
# ZeroGPU, which admits it when the caller's own allowance covers it.
MAX_GPU_SECONDS = 900


def gpu_seconds(audio_file_path: str, model_name: str) -> float:
    # Measured at about 18 s for 300 s with htdemucs, uploading included, and padded for the
    # slower mdx models
    return 10 + 0.15 * sf.info(audio_file_path).duration


# Separation Logic
@spaces.GPU(duration=gpu_seconds)
def separate_all_stems(audio_file_path: str, model_name: str):
    model = LOADED_MODELS[model_name]

    signal = AudioSignal(audio_file_path)
    original_sr = signal.sample_rate
    signal = signal.resample(44100)  # expects 44.1kHz

    # The model separates in stereo, so a mono input is duplicated to feed it
    is_mono = signal.num_channels == 1
    if is_mono:
        signal.audio_data = signal.audio_data.repeat(1, 2, 1)  # [batch, channels, samples]

    sr = signal.sample_rate

    # [batch, channels, samples], with the one item in the batch
    waveform = signal.audio_data.float()

    with torch.no_grad():
        stems_batch = apply_model(
            model,
            waveform.to(next(model.parameters()).device),
            overlap=0.2,
            shifts=1,
            split=True,
        )

    stems = stems_batch[0]

    output_signals = []
    for stem in stems:
        if is_mono:
            stem = stem.mean(dim=0, keepdim=True)
        signal = AudioSignal(stem.cpu().numpy().astype("float32"), sample_rate=sr)
        # Returned at the input's rate rather than the one the model runs at
        output_signals.append(signal.resample(original_sr))

    drums, bass, other, vocals = output_signals

    return [drums, bass, vocals, other]
```

- **`@spaces.GPU` reserves the GPU** for each call. `duration` is the longest a call may run, and
  takes either a number or a function of the same arguments as the decorated function. Demucs
  reserves 10 seconds plus 0.15 seconds per second of audio, which was measured on real songs and
  padded. A call that outlasts its reservation is stopped, and ZeroGPU admits a call only when the
  allowance of the caller covers it, so sizing the reservation to the request lets a short clip
  through where a fixed, generous one would not. Off ZeroGPU, the decorator does nothing.
  [Protecting the Caller's Allowance](/pyharp/guidelines#protecting-the-callers-allowance) gives
  the rules for sizing a reservation.
- **The input is converted for the model, and each stem converted back.** Demucs runs on 44.1 kHz
  stereo, so the function resamples the input and duplicates a mono input across two channels. It
  then returns each stem at the sample rate and channel count of the input.

## 5. Check the Request First

Begin the function that handles a request with its checks:

```python
# Process Function
def process_fn(audio_file_path, model_name):
    try:
        if model_name not in DEMUX_MODELS:
            raise gr.Error(f"Unsupported model selected: {model_name}")
        # Checked before the GPU is reserved, so a request the model cannot take spends none of
        # the caller's ZeroGPU allowance
        if not audio_file_path:
            raise gr.Error("Provide the mixture to separate.")
        try:
            duration = sf.info(audio_file_path).duration
        except Exception as exc:
            raise gr.Error(f"Could not read the audio file: {exc}") from exc
        if duration == 0:
            raise gr.Error("The audio file is empty.")
        if gpu_seconds(audio_file_path, model_name) > MAX_GPU_SECONDS:
            raise gr.Error(
                f"The mixture is {duration:.0f} s long, more than one request can separate in "
                f"the {MAX_GPU_SECONDS // 60} minutes it may run. Trim it."
            )
        output_signals = separate_all_stems(audio_file_path, model_name)
```

`process_fn` checks the input before `separate_all_stems` reserves a GPU, and refuses a song too
long to finish within the 15 minutes PyHARP allows a job. A refused request spends none of the
allowance of the caller, and HARP shows the message of its `gr.Error` in the error popup (see
[Error Reporting](/pyharp/guidelines#error-reporting)).

## 6. Keep the Format of the Input

Finish `process_fn`, at the indentation where step 5 left it, by writing each stem to a file:

```python
        # A lossless input keeps its container. A lossy one comes back as WAV, since encoding the
        # stems lossily again would expose the artifacts of the first encode and add a second set.
        input_ext = Path(audio_file_path).suffix.lower()
        extension = input_ext.lstrip(".") if input_ext in LOSSLESS else "wav"
        output_dir = Path(tempfile.mkdtemp())
        outputs = []
        for stem_name, signal in zip(STEM_NAMES, output_signals):
            output_path = output_dir / f"demucs_{model_name}_{stem_name.lower()}.{extension}"
            save_audio(signal, output_path)
            outputs.append(str(output_path))
        return tuple(outputs)
    except gr.Error:
        raise
    except Exception as e:
        raise gr.Error(str(e))
```

The stems already have the sample rate and channel count of the input, from step 4. Here each is
written in the container of the input as well, if that is WAV or FLAC. A result in the format of
its input replaces the original in a DAW session without conversion.
[Preserving the Input Format](/pyharp/guidelines#preserving-the-input-format) explains why a lossy
or AIFF input comes back as WAV. The last four lines turn any other failure into a `gr.Error`, so
that HARP has a message to show for it.

## 7. Add a Choice and More Outputs

End `app.py` with the components and the line that starts the app:

```python
# Gradio App
if __name__ == "__main__":
    with gr.Blocks() as demo:
        input_audio = gr.Audio(type="filepath", label="Input Audio").harp_required(True).set_info((
            "The mixture to separate. Mono input is duplicated to stereo first."
        ))

        model_dropdown = gr.Dropdown(
            choices=DEMUX_MODELS,
            label="Demucs Model",
            value="htdemucs",
            info=(
                "Choose which Demucs variant to use for separation:\n"
                "1. htdemucs: High-quality Hybrid Transformer Demucs trained on MusDB + 800 songs.\n"
                "2. mdx_extra: Trained with extended data. Balanced model offering a clear trade-off between quality and speed.\n"
                "3. mdx_extra_q: Quantized lightweight version of mdx_extra.\n"
            )
        )

        output_drums = gr.Audio(type="filepath", label="Drums").set_info("The isolated drums.")
        output_bass = gr.Audio(type="filepath", label="Bass").set_info("The isolated bass.")
        output_vocals = gr.Audio(type="filepath", label="Vocals").set_info("The isolated vocal.")
        output_instrumental = gr.Audio(type="filepath", label="Other").set_info((
            "Everything the other three stems did not take."
        ))

        app = build_endpoint(
            model_card=model_card,
            input_components=[input_audio, model_dropdown],
            output_components=[
                output_drums,
                output_bass,
                output_vocals,
                output_instrumental
            ],
            process_fn=process_fn
        )

    demo.queue().launch(pwa=True, show_error=True, ssr_mode=False)
```

- **The dropdown** offers the three variants, and describes each in `info`, which HARP shows on
  hover.
- **The four outputs** follow the four values `process_fn` returns, and become four tracks in HARP.
  PyHARP names each file after the model and the label of its output, _e.g._, `demucs_vocals.wav`.

## 8. Run It in HARP

The app is complete. Start it from its folder:

```bash
python app.py
```

The first start downloads the weights of all three variants, about 920 MB. Open
`http://localhost:7860` in HARP as a custom path, drag a song onto `Input Audio`, choose a variant,
and select `Process`. The stems fill the `Drums`, `Bass`, `Vocals`, and `Other` tracks.

## The Guidelines It Follows

The app applies the rules that our [deployment guidelines](/pyharp/guidelines) set for every
model, these among them:

- **Pin what the app installs and downloads**: PyHARP to a release tag, the packages of the model
  to exact versions, and the weights to a revision (see
  [Versions and Compatibility](/pyharp/guidelines#versions-and-compatibility)).
- **Run Python 3.10 or 3.12** on ZeroGPU, which supports no other version. The Space `README.md`
  sets it (see [Hosting Models](/hosting/spaces#deploying-example-2)).
- **Check the request before the GPU is reserved** (see
  [Protecting the Caller's Allowance](/pyharp/guidelines#protecting-the-callers-allowance)).
- **Keep the format of the input** (see
  [Preserving the Input Format](/pyharp/guidelines#preserving-the-input-format)).
- **Launch with `ssr_mode=False`**, since the server-side rendering it turns off can crash a Space
  as it starts (see [Structure of app.py](/pyharp/guidelines#structure-of-apppy)).

## Going Further

The other models we host show the rest of what an app can do, and each Space shows its code under
`Files`:

- **MIDI output**: [Transkun](https://huggingface.co/spaces/teamup-tech/Transkun) transcribes piano
  to a MIDI track, through a `gr.File` restricted to `.mid` and `.midi`.
- **Text input**: [text2midi](https://huggingface.co/spaces/teamup-tech/text2midi) generates MIDI
  from a `gr.Textbox`.
- **Optional inputs**: [MusicGen](https://huggingface.co/spaces/teamup-tech/MusicGen) takes an
  optional melody, marked with `.harp_required(False)`, which arrives in `process_fn` as `None` when
  left empty.
- **Other files**: [Whisper](https://huggingface.co/spaces/teamup-tech/Whisper) returns its
  transcript as a text file, through a generic `gr.File`.
- **Every component**: PyHARP's [UI Tester](https://github.com/TEAMuP-dev/pyharp/tree/main/examples/ui_tester)
  exercises every control, track, and label type HARP supports.

**Next:** [host the app](/hosting/spaces), so that it runs independently of your machine.
