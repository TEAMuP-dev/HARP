import argparse
from gradio_client import Client
from pathlib import Path
import json

def main(url, output_path, mode, ctrls_path=None):
    assert url, "Please specify a url to connect to."
    assert output_path, "Please specify an output path."
    client = Client(url)
    
    if mode == "get_ctrls":
        ctrls_path = client.predict(api_name="/wav2wav-ctrls")
        Path(ctrls_path).rename(output_path)
    elif mode == "predict":
        assert ctrls_path is not None, "Please specify a ctrls path."
        with open(ctrls_path) as f:
            ctrls = json.load(f)
            assert isinstance(ctrls, list), "Controls must be a list of parameter values."
        audio_path = client.predict(*ctrls, api_name="/wav2wav")
        Path(audio_path).rename(output_path)
    else:
        raise ValueError("Invalid mode. Choose either 'get_ctrls' or 'predict'.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Process some arguments.')
    parser.add_argument('--url', required=True, help='The URL to connect to.')
    parser.add_argument('--output_path', required=True, help='The output path to save the file.')
    parser.add_argument('--mode', required=True, choices=['get_ctrls', 'predict'], help='The mode of operation.')
    parser.add_argument('--ctrls_path', help='The path to the controls file.')
    
    args = parser.parse_args()
    
    main(args.url, args.output_path, args.mode, args.ctrls_path)
