from pyharp import *

import gradio as gr


model_card = ModelCard(
    name='<APP_NAME>',
    description='<APP_DESCRIPTION>',
    author='<APP_AUTHOR>',
    tags=['<APP>', '<TAGS>']
)

# <YOUR MODEL INITIALIZATION CODE HERE>


def process_fn(input_audio_path):
    """
    This function defines the audio processing steps

    Args:
        input_audio_path (str): the audio filepath to be processed.

        <YOUR_KWARGS>: additional keyword arguments necessary for processing.
            NOTE: These should correspond to and match order of UI elements defined below.

    Returns:
        output_audio_path (str): the filepath of the processed audio.
    """

    """
    <YOUR AUDIO LOADING CODE HERE>
    # Load audio at specified path using audiotools (Descript)
    signal = load_audio(input_audio_path)
    """

    """
    <YOUR AUDIO PROCESSING CODE HERE>
    # Perform a trivial operation (i.e. boosting)
    signal.audio_data = 2 * signal.audio_data
    """

    """
    <YOUR AUDIO SAVING CODE HERE>
    # Save processed audio and obtain default path
    output_audio_path = save_audio(signal, None)
    """

    return output_audio_path


# Build Gradio endpoint
with gr.Blocks() as demo:
    # Define Gradio Components
    components = [
        # <YOUR UI ELEMENTS HERE>
    ]

    # Build endpoint
    app = build_endpoint(model_card=model_card,
                         components=components,
                         process_fn=process_fn)

demo.queue()
demo.launch(share=True, show_error=True)
