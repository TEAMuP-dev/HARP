from pathlib import Path
import gradio as gr
from pyharp import ModelCard, build_endpoint

# Define the process function
def process_fn(input_audio, pitch_shift_amount):
    from audiotools import AudioSignal
    
    sig = AudioSignal(input_audio)
    sig.pitch_shift(pitch_shift_amount)

    output_dir = Path("_outputs")
    output_dir.mkdir(exist_ok=True)
    sig.write(output_dir / "output.wav")
    return sig.path_to_file

# Create a ModelCard
card = ModelCard(
    name="Pitch Shifter",
    description="A pitch shifting example for HARP.",
    author="Hugo Flores Garcia",
    tags=["example", "pitch shift"]
)


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
