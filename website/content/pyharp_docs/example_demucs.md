# Example 2: Stem Separation

This example walks through the app behind our
[Demucs](https://huggingface.co/spaces/teamup-tech/Demucs) model, which separates a song into
drums, bass, vocals, and other, and which [Using Hosted Models](/content/usage/models) runs in HARP.
It adds what larger models need to [Example 1](/content/pyharp_docs/example_skey): weights read from
a mirror on the Hub, a choice among several models built as the app starts, a GPU reserved for each request in proportion to its
length, and results in the format of the input.

The code shown is the deployed app, at revision
[`50db79a`](https://huggingface.co/spaces/teamup-tech/Demucs/tree/50db79a84aaaa0fe3346f23fe1b00f21f3b6526f).

## 1. Get the Code

```bash
mkdir demucs && cd demucs
python3 -m venv .venv
source .venv/bin/activate        # On Windows: .venv\Scripts\activate
pip install huggingface_hub
hf download teamup-tech/Demucs --repo-type space --local-dir . \
    --revision 50db79a84aaaa0fe3346f23fe1b00f21f3b6526f
pip install -r requirements.txt
```

The Space holds `app.py`, `requirements.txt`, and `README.md`. The requirements pin the `demucs`
package, and a release of PyTorch that ZeroGPU supports. Demucs also runs on a CPU, more slowly, so
the app works without a GPU.

## 2. Build the Models at Startup

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

The `demucs` package downloads the weights of a variant from a server of its authors the first time
it is loaded. The app reads them instead from a mirror on the Hub, pinned to one revision, and links
each checkpoint where `torch.hub` looks before downloading. It then builds and places all three
variants in the worker as the app starts, so that no request waits on a download or a build, which
on ZeroGPU would run inside its GPU reservation. The three fit in memory together. A family of
larger models would build its default here, and each other model the first time a request selects
it.

## 3. Reserve a GPU for Each Request

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
```

`duration` is the longest a call may run, and takes either a number or a function of the same
arguments as the decorated function. Demucs reserves 10 seconds plus 0.15 seconds per second of
audio, which was measured on real songs and padded. A call that outlasts its reservation is stopped,
and ZeroGPU admits a call only when the allowance of the caller covers it, so sizing the reservation
to the request lets a short clip through where a fixed, generous one would not. Off ZeroGPU, the
decorator does nothing.

## 4. Check the Request First

```python
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
allowance of the caller.

## 5. Keep the Input Format

```python
    signal = AudioSignal(audio_file_path)
    original_sr = signal.sample_rate
    signal = signal.resample(44100)  # expects 44.1kHz

    # The model separates in stereo, so a mono input is duplicated to feed it
    is_mono = signal.num_channels == 1
    if is_mono:
        signal.audio_data = signal.audio_data.repeat(1, 2, 1)  # [batch, channels, samples]

    sr = signal.sample_rate

# ...

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

# ...

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
```

Demucs runs on 44.1 kHz stereo, so the app resamples the input for the model, and duplicates a mono
input across two channels. It then returns each stem at the sample rate and channel count of the
input, and in the container of the input if that is WAV or FLAC. A result in the format of its input
drops back into the session unchanged.
[Preserving the Input Format](https://github.com/TEAMuP-dev/HARP/blob/main/docs/DEPLOYMENT.md#preserving-the-input-format)
explains why a lossy or AIFF input comes back as WAV.

## 6. Add a Choice and More Outputs

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

    demo.queue().launch(pwa=True, show_error=True)
```

- **The dropdown** offers the three variants, and describes each in `info`, which HARP shows on
  hover.
- **The four outputs** follow the four values `process_fn` returns, and become four tracks in HARP.
  PyHARP names each file after the model and the label of its output, _e.g._, `demucs_vocals.wav`.

## 7. Run It in HARP

```bash
python app.py
```

The first start downloads the weights of all three variants, about 920 MB. Open
`http://localhost:7860` in HARP as a custom path, drag a song onto `Input Audio`, choose a variant,
and select `Process`. The stems fill the `Drums`, `Bass`, `Vocals`, and `Other` tracks.

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
- **Every component**: the [UI Tester](https://github.com/TEAMuP-dev/pyharp/tree/main/examples/ui_tester)
  of PyHARP exercises every control, track, and label type HARP supports.

**Next:** [host the app](/content/hosting/spaces), so that it runs without your machine.
