# Hosting on Hugging Face Spaces

A [Space](https://huggingface.co/docs/hub/spaces-overview) runs an app on the hardware of Hugging
Face, at a permanent address, so that it works without your machine and others can use it. HARP
opens any public Space by its id, _e.g._, `teamup-tech/Demucs`, and every model on its Home tab is
a Space.

## Plans and Hardware

<!--@include: @/../docs/DEPLOYMENT.md#hugging-face-plans-->

On a paid plan, choose the hardware by measuring a request with ten seconds of input on a CPU. A
model faster than real time belongs on `cpu-basic`, where a request costs the user nothing. A slower
one belongs on ZeroGPU, which accelerates PyTorch alone.
[Choosing the Hardware](https://github.com/TEAMuP-dev/HARP/blob/main/docs/DEPLOYMENT.md#choosing-the-hardware)
gives the full rule.

## Deploying Example 2

The app from [Example 2](/content/pyharp_docs/example_demucs) is ready for ZeroGPU, so a free
account can host it too.

1. **Create the Space.** On [huggingface.co/new-space](https://huggingface.co/new-space), choose a
   name such as `demucs`, the `mit` license, the `Gradio` SDK with the blank template, `ZeroGPU`
   hardware, and public visibility, so that HARP can reach the Space without a key.

   <!-- TODO: screenshot. The "Create a new Space" form, filled in as listed above, with ZeroGPU
        selected as the hardware.
   ![Creating a Space with the Gradio SDK on ZeroGPU hardware](/images/space-create.png)
   -->

2. **Add the app.** Clone the Space, and copy `app.py`, `requirements.txt`, and `README.md` from
   Example 2 into it, replacing the generated `README.md`:

   ```bash
   git clone https://huggingface.co/spaces/<USERNAME>/demucs demucs-space
   cp demucs/app.py demucs/requirements.txt demucs/README.md demucs-space/
   ```

3. **Push.** Git asks for your username and, as the password, an
   [access token](https://huggingface.co/settings/tokens) with `Write` permission:

   ```bash
   cd demucs-space
   git add .
   git commit -m "Deploy Demucs"
   git push
   ```

4. **Watch it build.** The page of the Space shows the progress of the build, and `Logs` shows the
   output of installing the requirements and starting `app.py`, which is where an error appears if
   the Space fails to start. Once the status reads `Running`, try the app on the page.

   <!-- TODO: screenshot. The page of a running Space, with the Logs panel open on the container log.
   ![The page of a Space, with its logs open](/images/space-logs.png)
   -->

5. **Open it in HARP.** On the `Home` tab, click `Custom Path...`, enter `<USERNAME>/demucs`, and
   click `Load`.

The YAML at the top of `README.md` configures the Space:

```yaml
---
title: Demucs
emoji: ✂️
colorFrom: pink
colorTo: indigo
sdk: gradio
sdk_version: 6.24.0
python_version: "3.10"
app_file: app.py
pinned: false
license: mit
short_description: Demucs four-stem music source separation
tags:
  - category:separation
  - subcategory:stem-separation
  - input:audio
  - output:audio
  - sample-rate:44100
  - channels:stereo
  - stems
---
```

- **`sdk_version`** is the version of Gradio the Space installs, which is why `requirements.txt`
  does not list Gradio. HARP needs 6.13 or newer to report why a job failed.
- **`python_version`** is the Python the Space runs. ZeroGPU supports 3.10 and 3.12.
- **`license`** is the license of the model, which HARP shows on its badge.
- **`short_description`** and **`tags`** describe the model on the Home tab of HARP before it is
  loaded. The tags are the full list of the model card, including the `input:` and `output:` tags
  that PyHARP infers. Launch the app with `build_endpoint(..., show_controls=True)` and click
  `View Controls` to copy them.

[Space README](https://github.com/TEAMuP-dev/HARP/blob/main/docs/DEPLOYMENT.md#space-readme)
describes each field.

## ZeroGPU

A ZeroGPU Space has no GPU of its own. Each call to a function decorated with `@spaces.GPU` waits
for one, runs on it, and releases it, and the time it reserves comes out of the daily allowance of
the caller. HARP sends the Hugging Face key of the user, when one is set, with every request.

Example 2 shows what an app needs: `spaces` in `requirements.txt` and imported first, at least one
function decorated with `@spaces.GPU` (a ZeroGPU Space without one fails at startup), the model
placed on its device as the worker loads it, checks in `process_fn` before the GPU is reserved, a
`duration` sized to each request, and a `torch` release that ZeroGPU supports.
[Hardware Portability](https://github.com/TEAMuP-dev/HARP/blob/main/docs/DEPLOYMENT.md#hardware-portability)
and
[Protecting the Caller's Allowance](https://github.com/TEAMuP-dev/HARP/blob/main/docs/DEPLOYMENT.md#protecting-the-callers-allowance)
explain each rule.

## Gradio or Docker

A Space runs an app with one of two SDKs:

- **Gradio**, the default, builds the environment from `requirements.txt` with pip and
  `packages.txt` with apt, on the Python of `python_version`, with the Gradio of `sdk_version`. It is
  the only SDK that ZeroGPU supports.
- **Docker** builds the Space from a `Dockerfile` of your own, which leaves the environment entirely
  to you.

Choose Docker only when a model cannot share an environment with Gradio 6, and first check whether
an older release of Gradio 6 resolves the conflict, as it does for models tied to `transformers`
4.x (see [Dependency Conflicts](https://github.com/TEAMuP-dev/HARP/blob/main/docs/DEPLOYMENT.md#dependency-conflicts)).

Our [BeatNet](https://huggingface.co/spaces/teamup-tech/BeatNet/tree/main) Space is the example.
BeatNet relies on madmom, whose release supports Python 3.9 and an old NumPy, while Gradio 6 needs
Python 3.10 or newer, so the Space keeps two environments in one Docker image:

- **`Dockerfile`** builds a backend environment on Python 3.9, holding BeatNet and madmom from
  `requirements-backend-py39.txt`, and a frontend environment on Python 3.10, holding Gradio and
  PyHARP from `requirements-frontend-py310.txt`.
- **`backend_worker.py`** runs BeatNet on the file it is given, and prints the beats as one JSON
  object, to stdout alone.
- **`frontend_app.py`** is an ordinary PyHARP app, whose `process_fn` runs the backend with
  `subprocess.run`, parses its JSON, and returns the beats as labels.
- **`start.sh`** starts the frontend, which listens on `0.0.0.0` at the port that `app_port` names
  in `README.md`.

```python
completed = subprocess.run(
    [BACKEND_PYTHON, BACKEND_SCRIPT, audio_path],
    capture_output=True,
    check=False,
    text=True,
)
```

The call sets no timeout of its own, since PyHARP stops the subprocess along with the job when a
user cancels it or the job runs past its `timeout_s`. The
[Docker Spaces](https://github.com/TEAMuP-dev/pyharp#docker-spaces) section of the README of PyHARP
gives a template for the layout. A Docker Space needs a paid plan and cannot use ZeroGPU, so a model
that needs a GPU there has to pay for one.

## Maintaining a Space

- **Every push rebuilds the Space**, reinstalling its requirements, so pin each one to the version
  against which the app was tested.
- **Binary files**, such as weights or test audio, go through Git LFS, as the
  [Binary Files](https://github.com/TEAMuP-dev/pyharp#binary-files) section of the README of PyHARP
  describes. `hf upload` from the `huggingface_hub` package handles them without Git.
- **Keys and tokens** belong in the secrets of the Space, under `Settings`, which reach the app as
  environment variables. Never commit them.
- **A private Space** opens in HARP only with a Hugging Face key from an account that can see it.

## Listing on the Home Tab

HARP lists the public Spaces of the [TEAMuP organization](https://huggingface.co/teamup-tech) on its
Home tab, and describes each with the `short_description` and `tags` of its README until it is
loaded. A Space under any other account opens as a custom path. To propose a model for the Home tab,
open an [issue](https://github.com/TEAMuP-dev/HARP/issues).

## Hosting Elsewhere

An app also runs on any machine with Python, such as a lab workstation with a GPU, which keeps
private weights and audio on your own hardware. Run it there on a fixed port, and reach it from HARP
over an SSH tunnel as `http://localhost:<PORT>`, as the
[Self-Hosted Endpoints](https://github.com/TEAMuP-dev/pyharp#self-hosted-endpoints) section of the
README of PyHARP describes. HARP treats a local address as your own machine, and sends it your
Hugging Face key, so tunnel only to a server you trust with it.
