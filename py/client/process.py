from gradio_client import Client
from pathlib import Path
import json

import argbind


@argbind.bind(without_prefix=True)
def process(
    url: str = None, 
    output_path: str = None, 
    ctrls_path: str = None,
):
    assert url is not None, f"Please specify a url to connect to."
    assert output_path is not None, f"Please specify an output path."
    assert ctrls_path is not None, f"Please specify a ctrls path."

    client = Client(url)

    with open(ctrls_path) as f:
        ctrls = json.load(f)
        assert isinstance(ctrls, list), f"Controls must be a list parameter values."
    
    audio_path = client.predict(*ctrls, api_name="/wav2wav")

    Path(audio_path).rename(output_path)


if __name__ == "__main__":
    args = argbind.parse_args()

    with argbind.scope(args):
        process()