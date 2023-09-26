# PyHARP

PyHARP is a Python library designed to embed Gradio apps for audio processing in a Digital Audio Workstation (DAW) through the [HARP](https://github.com/audacitorch/HARP) plugin. 

PyHARP creates hosted, asynchronous, remote processing (HARP) endpoints in Digital Audio Workstations (DAWs), facilitating the integration of deep learning audio models into DAW environments through Gradio.

PyHARP makes use of [Gradio]() to provide a web endpoint for your python processing code, and [ARA]() to route audio from the DAW to the Gradio endpoint. 

## Why HARP? 
TODO: fill all the links

HARP is designed for processing audio in a DAW with deep learning models that are too large to run in real-time and/or on a user's CPU, or otherwise require a large offline context for processing. There are many examples of these kinds of models, like [EXAMPLES HERE]().  

HARP makes use of the [ARA]() framework to access all of the audio data in a track asynchronously, allowing for the audio to be processed offline by a remote server, like a [Gradio application], or a [HuggingFace Space]().

Other solutions for realtime processing with neural networks in DAWs (e.g. [NeuTone](https://neutone.space/)), these often require highly optimized models and rely on the model code to be [traced/scripted into a JIT representation](), which can sometimes be a challenge for the model developer.

PyHARP, on the other hand, relies on [Gradio](), which allows for the use of any Python function as a processing endpoint. PyHARP doesn't require for your deep learning code to be optimized or JIT compiled. PyHARP lets you process audio in a DAW using your deep learning library of choice. [Tensorflow]()? [PyTorch]()? [Jax]()? [Librosa]()? You pick. Hell, it doesn't even have to be deep learning code! Any arbitrary Python function can be used as a HARP processing endpoint, as long as it can be wrapped in a Gradio interface.

- Are you a deep learning researcher working on a new model and you'd like to give it a spin in a DAW and share it with your producer friends? Try HARP!
- Want to wrap your favorite librosa routine so you always have it handy when you're mixing? Try HARP! 
- Have a cool and quick audio processing idea and don't have the time to write C++ code for a plugin? Try HARP!
- Want to make an audio research demo that you'd like to put into an audio production workflow? Try HARP!

# Getting Started

## Download HARP
To use pyHARP apps in your DAW, you'll need the [HARP](https://github.com/audacitorch/harp) plugin. HARP is available as an ARA VST3 plugin, and can be used by any DAW host that supports the ARA framework. 


## Installation

```
git clone https://github.com/audacitorch/pyharp.git
cd pyharp
pip install -e .
```

## Examples
You can look at some on how to build HARP endpoints with PyHARP in the `examples/` directory. 
To run an example, you'll need to install that example's dependencies. 

For example, to run the pitch shifter example: 

```bash
cd examples/pitch_shifter
pip install -r requirements.txt
```

Here are some of the examples available:
- [Pitch Shifter](examples/pitch_shifter/)

# Tutorial: Making a HARP Harmonic/Percussive Separator.

To make a HARP app, you'll need to write some audio processing code and wrap it using Gradio and PyHARP.

## Write your processing code

Create a funciton which defines the processing logic for your audio model. This could be a source separation model, a text-to-music generation model, and music inpainting system, your handy dandy librosa processing routine, etc. 

**NOTE** This function should be a Gradio-compatible processing function, and should thus take the values of some input widgets as arguments. To work with HARP, the function should accept exactly ONE audio input argument + any number of other sliders, texboxes, etc. Additionally, the function should output exactly one audio file. 

For our pitch shifter, we'll use the [audiotools](https://github.com/descript/descript-audiotools) library. 
Our function  will take two arguments: 
- `input_audio`: the audio filepath to be processed
- `pitch_shift`: the amount of pitch shift to apply to the audio

```python
# Define the process function
def process_fn(input_audio, pitch_shift_amount):
    from audiotools import AudioSignal
    
    sig = AudioSignal(input_audio)
    sig.pitch_shift(pitch_shift_amount)

    output_dir = Path("_outputs")
    output_dir.mkdir(exist_ok=True)
    sig.write(output_dir / "output.wav")
    return sig.path_to_file # return the path to the output file
```

## Create a Model Card

A model card helps users identify your model and keep track of what it does. 
```python
from pyharp import ModelCard
# Create a ModelCard
card = ModelCard(
    name="Pitch Shifter",
    description="A pitch shifting example for HARP.",
    author="Hugo Flores Garcia",
    tags=["example", "pitch shift"]
)
```

## Create a Gradio Interface

Now, we'll create a Gradio interface for our processing function, connecting the input and output widgets to the function, and making our processing code accessible via a Gradio // HARP endpoint. 

To achieve this, we'll create a list of Gradio input widgets, as well as an audio output widget, then use the `build_endpoint` function from PyHARP to create a Gradio interface for our processing function. 

```python
# Build the endpoint
with gr.Blocks() as demo:

    # Define your Gradio interface
    inputs = [
        gr.Audio(
            label="Audio Input", 
            type="filepath"
        ), # make sure to have an audio input with type="filepath"!
        gr.Slider(
            minimum=-24, 
            maximum=24, 
            step=1, 
            value=7, 
            label="Pitch Shift (semitones)"
        ),
    ]
    
    # make an output audio widget
    output = gr.Audio(label="Audio Output", type="filepath")

    # Build the endpoint
    ctrls_data, ctrls_button, process_button = build_endpoint(inputs, output, process_fn, card)

demo.launch(share=True)
```

## Run the app

Now, we can run our app and test it out. 

```bash
python examples/pitch_shifter/pitch_shifter.py
```

This will create a local Gradio endpoint on `http://localhost:<PORT>`, as well as a forwarded gradio endpoint, with a format like `https://<RANDOM_ID>.gradio.live/`. You can copy that link and enter it in your HARP plugin to use your app in your DAW. 


Note that automatically generated Gradio endpoints are only available for 72 hours. If you'd like to keep your endpoint alive and share it with other users, you can easily create a [HuggingFace Space](https://huggingface.co/docs/hub/spaces-overview) to host your HARP app indefinitely, or alternatively, host your gradio app using other hosting services.  

