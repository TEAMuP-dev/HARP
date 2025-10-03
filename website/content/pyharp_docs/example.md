# Example PyHARP App: Speech Separation with TIGER

To get the hang of building PyHARP applications, we'll deploy a state-of-the-art neural network model within HARP in just a few lines of code.

### TIGER: a Powerful and Lightweight Speech Separation Model

DAW users such as video editors and podcasters often work with noisy speech recordings captured in less-than-ideal conditions, and "cleaning up" these recordings can be difficult and time-intensive. Luckily, there are a number of cutting-edge deep learning models capable of automating this clean-up process by separating a speech signal from any background sounds in a recording, including environment noise and even other speakers. In this tutorial, we'll look at [TIGER](https://arxiv.org/abs/2410.01469), a lightweight speech separation model capable of running efficiently on a laptop CPU. This means that you won't need a GPU for this tutorial -- just working HARP and PyHARP installations!

To get started, we'll install [the code](https://github.com/JusperLee/TIGER) needed to run TIGER:

```
git clone https://github.com/JusperLee/TIGER.git
cd TIGER && pip install -r requirements.txt
```

Next, we'll look at how TIGER processes audio files to separate speech. From [`TIGER/inference_speech.py`](https://github.com/JusperLee/TIGER/blob/main/inference_speech.py), we can see that TIGER requires audio files to be resampled to 16kHz and formatted as a PyTorch tensor of shape `[1, C, T]` where `C` is the number of channels and `T` is the number of audio samples; then, the model can be applied in one line of code:


```
# Pass the prepared input tensor to the model
ests_speech = model(audio_input)  # Expected output shape: [1, num_spk, T]
```

Here, `num_spk` is the estimated number of speakers in the recording. For our example application, we'll only return audio of the first speaker.

```
# Select audio of first speaker
output = ests_speech[:, 0, :]  # Expected output shape: [1, T]
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
    tags=["example", "speech separation"],
    midi_in=False,
    midi_out=False
  )
  ```
* We need to define a list of __Gradio interactive components__ specifying the interface. In our case, because we only require audio input/output, we'll leave this list empty:
  ```
  # Define Gradio Components
  components = []
  ```
* We need to define a __processing function__ for handling file input and output. This function will load an audio file from a given path, format it for TIGER and run separation as discussed above, save the output to a new file, and return the path of this output file.
  ```
  # Define the processing function
  @torch.inference_mode()
  def process_fn(input_audio_path):

    # By default, load audio as a Descript-AudioTools `AudioSignal` object
    sig = load_audio(input_audio_path)  # Wraps a tensor of shape [1, C, T]

    audio_input = sig.resample(16_000).audio_data  # Tensor of shape [1, C, T]

    # Apply TIGER
    ests_speech = model(audio_input)  # Expected output shape: [1, num_spk, T]
    output = ests_speech[:, 0, :]  # Expected output shape: [1, T]

    sig.audio_data = output
    output_audio_path = save_audio(sig)

    # Because this application does not apply labels to audio, return an empty
    # label list
    output_labels = LabelList()

    return output_audio_path, output_labels
  ```

Adding imports and some model-loading code, our final `app.py` should look like this:

```
from pyharp import *

import yaml
import os
import gradio as gr
import torchaudio
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
    tags=["example", "speech separation"],
    midi_in=False,
    midi_out=False
)

# Define the processing function
@torch.inference_mode()
def process_fn(input_audio_path):

    # By default, load audio as a Descript-AudioTools `AudioSignal` object
    sig = load_audio(input_audio_path)  # Wraps a tensor of shape [1, C, T]
    
    audio_input = sig.resample(16_000).audio_data.to(device)  # Tensor of shape [1, C, T]
    
    # Apply TIGER
    ests_speech = model(audio_input)  # Expected output shape: [1, num_spk, T]
    output = ests_speech[:, :1, :]  # Expected output shape: [1, 1, T]
    
    sig.audio_data = output
    output_audio_path = save_audio(sig.cpu())  # Ensure our output signal is on CPU
    
    # Because this application does not apply labels to audio, return an empty
    # label list
    output_labels = LabelList()
    
    return output_audio_path, output_labels

# Build Gradio endpoint
with gr.Blocks() as demo:
    # Define Gradio Components
    components = []

    app = build_endpoint(model_card=model_card,
                         components=components,
                         process_fn=process_fn)

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