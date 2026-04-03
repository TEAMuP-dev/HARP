# Example PyHARP App: Speech Separation with TIGER

To get the hang of building PyHARP applications, we'll deploy a state-of-the-art neural network model within HARP in just a few lines of code.

### TIGER: a Powerful and Lightweight Speech Separation Model

DAW users such as video editors and podcasters often work with noisy speech recordings captured in less-than-ideal conditions, and "cleaning up" these recordings can be difficult and time-intensive. Luckily, there are a number of cutting-edge deep learning models capable of automating this clean-up process by separating a speech signal from any background sounds in a recording, including environment noise and even other speakers. In this tutorial, we'll look at [TIGER](https://arxiv.org/abs/2410.01469), a lightweight speech separation model capable of running efficiently on a laptop CPU. This means that you won't need a GPU for this tutorial -- just working HARP and PyHARP installations!

To get started, we'll install [the code](https://github.com/JusperLee/TIGER) needed to run TIGER:

```
git clone https://github.com/JusperLee/TIGER.git
cd TIGER
```

In the `requirements.txt`, remove `triton==3.1.0` to avoid dependency issues. Then pip install:

```
pip install -r requirements.txt
```

Next, we'll look at how TIGER processes audio files to separate speech. From [`TIGER/inference_speech.py`](https://github.com/JusperLee/TIGER/blob/main/inference_speech.py), we can see that TIGER requires audio files to be resampled to 16kHz and formatted as a PyTorch tensor of shape `[1, C, T]` where `C` is the number of channels and `T` is the number of audio samples; then, the model can be applied in one line of code:


```
# Pass the prepared input tensor to the model
ests_speech = model(audio_input)  # Expected output shape: [1, num_spk, T]
```

Here, `num_spk` is the estimated number of speakers in the recording. The TIGER model sets `num_spk` to two. We are going to take both outputs from the model:

```
# Select audio of first speaker
output_1 = ests_speech[:, 0, :]  # Expected output shape: [1, T]
output_2 = ests_speech[:, 1, :]  # Expected output shape: [1, T]
```

And that's it -- to deploy TIGER in HARP, we need to write a Gradio application for processing audio files like this, sprinkle in some PyHARP functions, and then run our application in the background to handle any audio sent by HARP.

### Writing a HARP-Compatible Gradio Endpoint


Now that we have a grip on how to run TIGER, let's revisit the [elements](/content/pyharp_docs/pyharp_app.html) of a HARP-compatible Gradio application:

* We need to define a __model card__ describing the underlying model:
  ```
  # Create a ModelCard
  model_card = ModelCard(
    name="TIGER",
    description="The TIGER speech separation model of Xu et al. (https://arxiv.org/abs/2410.01469)",
    author="Your name",
    tags=["example", "speech separation"]
  )
  ```
* We need to define a list of __Gradio interactive components__ specifying the interface. In our case, we need one audio input and two audio outputs:
  ```
  # Define Gradio Components
  # Input
  input_audio = gr.Audio(
      label="Input Audio",
      type="filepath",
      sources=["upload", "microphone"]
  )
  # Outputs
  output_audio_1 = gr.Audio(
      type="filepath",
      label="Output Audio 1"
  )
  output_audio_2 = gr.Audio(
      type="filepath",
      label="Output Audio 2"
  )
* Then we define a __processing function__ for handling file input and output. This function will load an audio file from a given path, format it for TIGER and run separation as discussed above, save the outputs to new files, and return the paths of the output files. Note that the parameters of the function correspond to the input components and the return values correspond to the output components.
  ```
  # Define the processing function
  @torch.inference_mode()
  def process_fn(input_audio_path):

    # By default, load audio as a Descript-AudioTools `AudioSignal` object
    sig = load_audio(input_audio_path)  # Wraps a tensor of shape [1, C, T]

    audio_input = sig.resample(16_000).audio_data.to(device)  # Tensor of shape [1, C, T]

    # Apply TIGER
    ests_speech = model(audio_input)  # Expected output shape: [1, num_spk, T]
    output_1 = ests_speech[:, 0, :]  # Expected output shape: [1, T]
    output_2 = ests_speech[:, 1, :]

    # Create two new audio
    sig_1 = AudioSignal(output_1.cpu().numpy().astype("float32"), sample_rate=16000)
    sig_2 = AudioSignal(output_2.cpu().numpy().astype("float32"), sample_rate=16000)

    # save to files
    output_dir = Path("_outputs").resolve()
    output_dir.mkdir(exist_ok=True, parents=True)

    output_audio_path_1 = output_dir / "sig1.wav"
    output_audio_path_2 = output_dir / "sig2.wav"
    
    save_audio(sig_1, output_audio_path_1)
    save_audio(sig_2, output_audio_path_2)

    return output_audio_path_1, output_audio_path_2
  ```
* After we define the __model card__, __I/O components__ and the __processing function__, we aggregate them into an endpoint:
  ```
  # Build Endpoint
  app = build_endpoint(
      model_card=model_card,
      input_components = [input_audio],
      output_components = [output_audio_1, output_audio_2],
      process_fn=process_fn
  )
  ```

Adding imports and model-loading code, our final `app.py` should look like this:

```
from pyharp import *
from audiotools import AudioSignal

import os
from pathlib import Path
import gradio as gr
import torch

import look2hear.models

# Config
cache_dir = "cache"
device = torch.device("cuda") if torch.cuda.is_available() else torch.device("cpu")  # Use GPU only if available
print(f"Using device: {device}")

# Load TIGER model
if cache_dir:
    os.makedirs(cache_dir, exist_ok=True)
model = look2hear.models.TIGER.from_pretrained("JusperLee/TIGER-speech", cache_dir=cache_dir)
model.to(device)
model.eval()

# Create a ModelCard
model_card = ModelCard(
    name="TIGER",
    description="The TIGER speech separation model of Xu et al. (https://arxiv.org/abs/2410.01469)",
    author="Your name",
    tags=["example", "speech separation"]
)

# Define the processing function
@torch.inference_mode()
def process_fn(input_audio_path):

    # By default, load audio as a Descript-AudioTools `AudioSignal` object
    sig = load_audio(input_audio_path)  # Wraps a tensor of shape [1, C, T]
    
    audio_input = sig.resample(16_000).audio_data.to(device)  # Tensor of shape [1, C, T]
    
    # Apply TIGER
    ests_speech = model(audio_input)  # Expected output shape: [1, num_spk, T]
    output_1 = ests_speech[:, 0, :]  # Expected output shape: [1, 1, T]
    output_2 = ests_speech[:, 1, :]

    sig_1 = AudioSignal(output_1.cpu().numpy().astype("float32"), sample_rate=16000)
    sig_2 = AudioSignal(output_2.cpu().numpy().astype("float32"), sample_rate=16000)

    output_dir = Path("_outputs").resolve()
    output_dir.mkdir(exist_ok=True, parents=True)

    output_audio_path_1 = output_dir / "sig1.wav"
    output_audio_path_2 = output_dir / "sig2.wav"
    
    save_audio(sig_1, output_audio_path_1)
    save_audio(sig_2, output_audio_path_2)
    
    return output_audio_path_1, output_audio_path_2

# Build Gradio endpoint
with gr.Blocks() as demo:
    # Define Gradio Components
    input_audio = gr.Audio(
        label="Input Audio",
        type="filepath",
        sources=["upload", "microphone"]
    )
    output_audio_1 = gr.Audio(
        type="filepath",
        label="Output Audio 1"
    )
    output_audio_2 = gr.Audio(
        type="filepath",
        label="Output Audio 2"
    )
    # output_labels = gr.JSON(label="Separation")

    app = build_endpoint(
        model_card=model_card,
        input_components = [input_audio],
        output_components = [output_audio_1, output_audio_2], # , output_labels],
        process_fn=process_fn
    )

demo.queue()
demo.launch(share=True, show_error=True)
```

### Deploying Our App

For now, we'll put `app.py` inside the `TIGER/` directory to avoid further installation steps. Note that while PyHARP's utilities use [Descript-AudioTools](https://github.com/descriptinc/audiotools) under the hood to handle audio loading and saving, you're free to use whichever libraries you want as long as they can read and produce valid audio files.

With our application up and running, it's time to link it to HARP. Run:

```
python app.py
```

Your terminal should display something like this:

```
* Running on local URL:  http://127.0.0.1:7860
* Running on public URL: https://8661b0cf18d5cf17ec.gradio.live

This share link expires in 1 week. For free permanent hosting and GPU upgrades, run `gradio deploy` from the terminal in the working directory to deploy to Hugging Face Spaces (https://huggingface.co/spaces)
```

Copy the public URL and open up HARP. Paste the URL as a `custom path...`, click `Load`, and voila -- you should now be able to run state-of-the-art speech separation in your DAW!