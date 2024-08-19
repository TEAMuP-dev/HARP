from audiotools import AudioSignal
from pyharp import *

import gradio as gr
import librosa
import torch


def hpss(signal: AudioSignal, **kwargs):
    h, p = librosa.effects.hpss(signal.audio_data.squeeze().numpy(), **kwargs)

    if h.ndim == 1:
        h = h[None, None, :]
        p = p[None, None, :]
    elif h.ndim == 2:
        h = h[None, :, :]
        p = p[None, :, :]
    else:
        assert False

    harmonic_signal = signal.clone()
    harmonic_signal.audio_data = torch.from_numpy(h)

    percussive_signal = signal.clone()
    percussive_signal.audio_data = torch.from_numpy(p)

    return harmonic_signal, percussive_signal


MIN_DB = -120

def process_fn(audio_file_path,
               harmonic_db: float, 
               percussive_db: float, 
               kernel_size: int = 31, 
               margin: float = 1.0):
    sig = load_audio(audio_file_path)
    
    harmonic, percussive = hpss(sig, kernel_size=int(kernel_size), margin=margin)

    def clip(db):
        if db == MIN_DB:
            db = -float("inf")

        return db

    # mix the signals, apply gain
    sig = (
        harmonic.volume_change(clip(harmonic_db)) 
        + percussive.volume_change(clip(percussive_db))
    )

    output_audio_path = save_audio(sig, None)

    return output_audio_path
    
# Create a ModelCard
model_card = ModelCard(
    name="Harmonic / Percussive Separation",
    description="Remix a Track into its harmonic and percussive components.",
    author="Hugo Flores Garcia",
    tags=["example", "separator", "hpss"]
)


# Build the endpoint
with gr.Blocks() as demo:
    # Define your Gradio interface
    components = [
        gr.Slider(
            minimum=MIN_DB, maximum=24, 
            step=1, value=0, 
            label="Harmonic Level (dB)"
        ),
        gr.Slider(
            minimum=MIN_DB, maximum=24, 
            step=1, value=0, 
            label="Percussive Level (dB)"
        ),
        gr.Slider(
            minimum=1, maximum=101, 
            step=1, value=31, 
            label="Kernel Size"
        ),
        gr.Slider(
            minimum=0.5, maximum=5.0, 
            step=0.1, value=1.0, 
            label="Margin"
        ),
    ]

    # Build the endpoint
    app = build_endpoint(model_card=model_card,
                         components=components,
                         process_fn=process_fn)

demo.queue()
demo.launch(share=True, show_error=True)
