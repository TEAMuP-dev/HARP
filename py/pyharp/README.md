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

