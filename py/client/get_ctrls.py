from gradio_client import Client
from pathlib import Path

import argbind


@argbind.bind(without_prefix=True)
def main(url: str = None, output_path: str = None):
    assert url is not None, f"Please specify a url to connect to."
    assert output_path is not None, f"Please specify an output path."
    client = Client(url)
    ctrls_path = client.predict(api_name="/wav2wav-ctrls")

    Path(ctrls_path).rename(output_path)


if __name__ == "__main__":
    args = argbind.parse_args()

    with argbind.scope(args):
        main()