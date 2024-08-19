from pyharp import *

import gradio as gr
import torchaudio
import torch

# Define the process function
@torch.inference_mode()
def process_fn(input_audio_path, pitch_shift_amount):

    if isinstance(pitch_shift_amount, torch.Tensor):
        pitch_shift_amount = pitch_shift_amount.long().item()

    sig = load_audio(input_audio_path)

    ps = torchaudio.transforms.PitchShift(
        sig.sample_rate,
        n_steps=pitch_shift_amount, 
        bins_per_octave=12, 
        n_fft=512
    ) 
    sig.audio_data = ps(sig.audio_data)

    output_audio_path = save_audio(sig)

    return output_audio_path

# Create a ModelCard
model_card = ModelCard(
    name="Pitch Shifter",
    description="A pitch shifting example for HARP.",
    author="Hugo Flores Garcia",
    tags=["example", "pitch shift"],
    midi_in=False,
    midi_out=False
)


# Build the endpoint
with gr.Blocks() as demo:
    # Define your Gradio interface
    components = [
        gr.Slider(
            minimum=-24, 
            maximum=24, 
            step=1, 
            value=7, 
            label="Pitch Shift (semitones)"
        ),
    ]

    app = build_endpoint(model_card=model_card,
                         components=components,
                         process_fn=process_fn)

demo.queue()
demo.launch(share=True, show_error=True)
