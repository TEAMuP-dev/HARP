# Using Third-Party APIs

Some models are closed-source and offered only through an API, such as a commercial service, and
others run on a Hugging Face Space that cannot be reproduced elsewhere. HARP reaches them in three
ways, which differ in whose key pays for each request, and in whether HARP itself has to change:

- **[A PyHARP app that calls the API](#wrapping-an-api)**, with a key that belongs to the host of
  the app, who pays for every request. HARP needs no change.
- **[A PyHARP app that calls another Space](#calling-another-space)**, forwarding the allowance of
  each user, who pays for their own requests. HARP needs no change.
- **[A provider built into HARP](#adding-a-provider)**, with a key each user adds to HARP, who pays
  for their own requests. This takes a new client class in HARP.

## Wrapping an API

A PyHARP app can do anything in `process_fn`, including sending the input to a web API and
returning what comes back. HARP sees an ordinary model, with the controls and tracks the app
declares, and the app can be hosted anywhere an app can.

The sketch below sends audio to a hypothetical service at `api.example.com`. Most services follow
the same shape, or provide a Python package that wraps it:

```python
from pyharp import get_default_path

import gradio as gr
import os
import requests


API_URL = "https://api.example.com/v1/process"

# A secret of the Space, which reaches the app as an environment variable. Never commit a key.
API_KEY = os.environ["EXAMPLE_API_KEY"]


def process_fn(input_audio_path: str, strength: float) -> str:
    with open(input_audio_path, "rb") as audio:
        response = requests.post(
            API_URL,
            headers={"Authorization": f"Bearer {API_KEY}"},
            files={"audio": audio},
            data={"strength": strength},
            timeout=600,
        )

    # The explanation from the service is the most useful thing to show the user
    if response.status_code != 200:
        raise gr.Error(f"The service refused the request ({response.status_code}): {response.text}")

    output_audio_path = get_default_path(ext=".wav")

    with open(output_audio_path, "wb") as output:
        output.write(response.content)

    return output_audio_path
```

On a Space, add the key as a secret under `Settings > Variables and secrets`, which keeps it out of
the repository and out of any copy of the Space. A Space can make requests over ports 80, 443, and
8080 alone.

The key belongs to whoever hosts the app, so every request any user makes is billed to that account.
Consider the cost before making such an app public, and check that the terms of the service permit
serving others through your key.

## Calling Another Space

A PyHARP app can also call another Gradio app with
[`gradio_client`](https://www.gradio.app/guides/getting-started-with-the-python-client), _e.g._,
where the authors of a model serve it on a Space of their own that cannot be reproduced elsewhere.
When that Space runs on ZeroGPU, forward the `x-ip-token` header of the caller, which Hugging Face
adds to every request a Space receives. ZeroGPU then bills the allowance of each user, as a direct
call would, rather than one allowance shared through the calling Space:

```python
from gradio.context import LocalContext
from gradio_client import Client


def process_fn(input_audio_path: str) -> str:
    # PyHARP restores the request inside its worker, so the header can be read from there
    request = LocalContext.request.get(None)
    ip_token = request.headers.get("x-ip-token", "") if request else ""

    client = Client("https://owner-backend.hf.space", headers={"x-ip-token": ip_token})

    ...
```

Prefer hosting the model yourself wherever its license and its hardware allow, since the owners of
the other Space can change its API, or take it down, without notice.
[Calling Another Space](https://github.com/TEAMuP-dev/HARP/blob/main/docs/DEPLOYMENT.md#calling-another-space)
covers the cases where it is the right choice, and what such an app owes HARP.

## Adding a Provider

HARP can also call a service directly, without an app in between, as it does for the Stable Audio
models of Stability AI. Each user adds their own key under `File > Settings > API Keys`, HARP sends
it only to that service, and each request is billed to that user. The controls of each model are
described in a file that ships with HARP, so the model appears on the Home tab of every user without
being hosted anywhere.

This route takes a change to HARP itself: a client class that translates requests from HARP into
the API of the service, released with a new version of HARP. It is the route for a model that is
closed-source but offered through an API, since nothing else can reach it, and it suits a service
with which many users already have an account.
[Adding an API Provider](/content/contributing/add_provider) describes the steps.
