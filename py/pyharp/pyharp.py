from typing import List
from dataclasses import dataclass, asdict

import gradio as gr

@dataclass
class Ctrl:
    label: str

    def __str__(self) -> str:
        return f"{self.ctrl_type}({self.label})"
    
    def to_dict(self) -> dict:
        d = asdict(self)
        d['ctrl_type'] = self.ctrl_type
        return d

@dataclass
class SliderCtrl(Ctrl):
    minimum: float
    maximum: float
    step: float
    value: float
    ctrl_type: str = "slider"

    def __str__(self) -> str:
        return super().__str__() + f"({self.minimum}, {self.maximum}, {self.step}, {self.value})"

@dataclass
class TextCtrl(Ctrl):
    value: str
    ctrl_type: str = "text"

    def __str__(self) -> str:
        return super().__str__() + f"({self.value})"


@dataclass
class AudioInCtrl(Ctrl):
    ctrl_type: str = "audio_in"


@dataclass
class ModelCard:
    name: str
    description: str
    author: str
    tags: List[str]

    def __str__(self) -> str:
        return f"ModelCard({self.name}, {self.description}, {self.author}, {self.tags})"



def build_ctrls(inputs: list) -> List[Ctrl]:
    """Builds a list of Ctrl objects based on Gradio input controls.

    Args:
        inputs (list): A list of Gradio input controls.
            NOTE: The API must contain exactly ONE gr.Audio widget for use as input. It's crucial that the order
            of inputs matches the order in the Gradio UI to ensure proper alignment when communicating with
            the HARP client. Currently, HARP supports gr.Slider, gr.Textbox, and gr.Audio widgets as inputs.


    Returns:
        List[Ctrl]: A list of Ctrl objects encapsulating the Gradio input controls' metadata.

    Raises:
        AssertionError: If inputs is not a list.
        ValueError: If a Gradio input control is not supported.
    """

    assert isinstance(inputs, list), f"inputs must be a list, not {type(inputs)}"

    ctrls = []
    for _in in inputs:
        print(f"processing {_in}")
        if isinstance(_in, gr.Slider):
            ctrl = SliderCtrl(
                minimum=_in.minimum,
                maximum=_in.maximum,
                label=_in.label, 
                value=_in.value, 
                step=_in.step,
            )
        elif isinstance(_in, gr.Audio):
            assert _in.type == "filepath", f"Audio input must be of type filepath, not {_in.type}"
            ctrl = AudioInCtrl(
                label=_in.label
            )
        elif isinstance(_in, gr.Textbox):
            ctrl = TextCtrl(
                label=_in.label,
                value=_in.value
            )
        else:
            raise ValueError(
                f"HARP does not support {_in}. Please remove this control or use an alternative one."
            )
        
        print(f"adding {ctrl}")
        ctrls.append(ctrl)
    
    # check that we have exactly one audio input
    audio_inputs = [c for c in ctrls if isinstance(c, AudioInCtrl)]
    if len(audio_inputs) != 1:
        raise ValueError(
            f"HARP requires exactly one audio input. You have {len(audio_inputs)}."
        )

    return ctrls
    

def build_endpoint(
        inputs: list, 
        output: gr.Audio, 
        process_fn: callable, 
        card: ModelCard, 
        visible: bool = True
    ) -> tuple:
    """Builds a Gradio endpoint compatible with HARP, facilitating VST3 plugin usage in a DAW.

    Args:
        inputs (Union[list]): Gradio input widgets.
            NOTE: The API must contain exactly ONE gr.Audio widget for use as input. It's crucial that the order
            of inputs matches the order in the Gradio UI to ensure proper alignment when communicating with
            the HARP client. Currently, HARP supports gr.Slider, gr.Textbox, and gr.Audio widgets as inputs.

        output (gr.Audio): Gradio output audio widget.
        process_fn (callable): Function processing the inputs to generate the output.
        card (ModelCard): A ModelCard object describing the model.
        visible (bool, optional): Specifies visibility of the endpoint in the Gradio UI. Defaults to True.

    Returns:
        tuple: A tuple containing: 
            1. A gr.JSON to store the ctrl data.
            2. A gr.Button to get the ctrl data.
            3. A gr.Button to process the input and generate the output.

    """
    # gather a list of ctrls from the Gradio input widgets
    ctrls = build_ctrls(inputs)
    assert isinstance(output, gr.Audio), f"output must be a gr.Audio widget, not {type(output)}"
    assert output.type == "filepath", f"output Audio widget must be of type filepath, not {output.type}"

    # build a callable with no inputs that returns the ctrls and card
    def fn():
        out = {
            "ctrls": [ctrl.to_dict() for ctrl in ctrls], 
            "card": asdict(card)
        }
        print(out)
        return out

    # gradio widget to store the ctrl data
    ctrls_data = gr.JSON(label="ctrls")

    # the endpoint itself
    ctrls_button = gr.Button("(HARP) get_ctrls", visible=visible)
    ctrls_button.click(
        fn=fn, 
        inputs=[], 
        outputs=ctrls_data,
        api_name="wav2wav-ctrls"
    )

    process_button = gr.Button("(HARP) process", visible=visible)
    process_button.click(
        fn=process_fn, 
        inputs=inputs, 
        outputs=[output],
        api_name="wav2wav"
    )

    return ctrls_data, ctrls_button, process_button


