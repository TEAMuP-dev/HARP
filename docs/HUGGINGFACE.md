# Hugging Face Changes

A log of the changes to Hugging Face, ZeroGPU, and Gradio that broke part of this project or forced
a change to it, newest first. [DEPLOYMENT.md](DEPLOYMENT.md) states the current terms, which each
entry links rather than restates.

## How Hugging Face Affects HARP

HARP runs no model itself. Every model on its Home tab, other than those of Stability AI, is a Space
of the `teamup-tech` organization, about half of them on ZeroGPU, and HARP reaches each one through
the Gradio API. PyHARP is built on Gradio, and the validation harness on `gradio_client` and
`huggingface_hub`. Hugging Face therefore decides where our models run, what a request costs the
user who sends it, and what a Space can install, and it changes these with little or no notice. Each
change logged below broke Spaces, the harness, or the way we document deploying a model, with nothing
changed in this repository, and further changes should be expected to arrive the same way.

These are the points at which the project depends on Hugging Face, and where to look first when
something breaks without a change of our own:

- **Hosting.** Which accounts can host which Spaces, on what hardware, decides how a contributor can
  deploy a model (see [Choosing the Hardware](DEPLOYMENT.md#choosing-the-hardware)).
- **ZeroGPU.** Its GPUs, the PyTorch releases it accepts, how it admits and bills a reservation, and
  the daily allowance of each caller decide whether a request runs at all. HARP and the harness
  recognize ZeroGPU by the `zero-` prefix of its hardware names, and a quota error by its wording:
  the word "quota" in `src/clients/GradioClient.h`, and `QUOTA_EXHAUSTED_MARKERS` in the harness.
- **Builds.** Every commit rebuilds a Space with the current default Python, the Gradio that
  `sdk_version` names, and whatever its unpinned requirements resolve to that day, so a change can
  surface long after it happened.
- **Gradio.** HARP calls the `/gradio_api/call/` endpoints and reports the error payloads that Gradio
  6.13 and newer forward. PyHARP requires Gradio 6.13 or newer, and the harness drives every Space
  through `gradio_client`.
- **The Hub API.** HARP fills its Home tab from one request listing the Spaces of the organization,
  with their README metadata and runtime stage, and the harness reads the same stages to wake and
  restart Spaces.

## Catching a Change

- **The daily validation is the first signal.** Many Spaces failing the same way on one day, or a
  Space failing at build with nothing changed, points at the platform rather than the Spaces (see
  the [validation README](../model_validation/README.md)).
- **Pins delay a change but cannot stop one.** A pinned Space still runs on the hardware, Python,
  and terms that Hugging Face provides when it next rebuilds.
- **Hugging Face announces few changes.** They show first in the
  [ZeroGPU documentation](https://huggingface.co/docs/hub/spaces-zerogpu), on the
  [forum](https://discuss.huggingface.co/), and in the release histories of `gradio`,
  `gradio_client`, `huggingface_hub`, and `spaces` on PyPI.

## Adding an Entry

Add an entry, newest first, whenever part of the project breaks for a reason outside this repository
and the Spaces. Say what changed and when, with the earliest evidence found, how it affected the
project, and what we did. Quote the error the change produces, so that searching for it finds the
entry.

## Log

### October 2026: CPU Spaces Build on Python 3.13

**Change.** A Gradio Space that sets no `python_version` builds on Python 3.13 on CPU hardware and on
3.10.13 on ZeroGPU, although the
[configuration reference](https://huggingface.co/docs/hub/spaces-config-reference) gives 3.10 as the
default. When the CPU default moved is unknown. It was first noticed on October 23, and build logs
confirmed it on October 25.

**Effect.** Nothing has broken yet, since every such Space resolves for 3.13 today. Fourteen CPU
Spaces of the fleet set no `python_version`, so the next rebuild of any of them installs for 3.13,
where a package that does not support it fails.

**Response.** The lint resolves the requirements of such a Space for 3.13, which fails the Space
before a rebuild would (see [Linting Spaces](../model_validation/README.md#linting-spaces)).

### July 2026: Free Accounts Lose CPU Spaces

**Change.** Hosting a Gradio or Docker Space on `cpu-basic` now takes a paid plan, and a free account
is told that "hosting Gradio and Docker Spaces on free cpu-basic requires a PRO subscription". A free
personal account can host Static Spaces, and up to two ZeroGPU Spaces once it is 30 days old and its
email is verified. Hugging Face made no announcement. The first reports date from July 8, and the
[ZeroGPU documentation](https://huggingface.co/docs/hub/spaces-zerogpu#hosting-limitations) now
states the limits.

**Effect.** The fleet was unaffected, since `teamup-tech` is on a Team plan. The way we document
deploying a model narrowed: a contributor with a free account can host only on ZeroGPU, only once the
account is 30 days old, and only with a function decorated with `@spaces.GPU`, even for a model that
runs on CPU, since ZeroGPU fails to start a Space without one.

**Response.** [Choosing the Hardware](DEPLOYMENT.md#choosing-the-hardware) lists what each account
can host, [Hardware Portability](DEPLOYMENT.md#hardware-portability) keeps the decorator where a free
account hosts the Space, and the [hosting page](https://harp.tools/content/hosting/spaces.html) of
the website deploys its example on ZeroGPU, which a free account can host.

**Sources.** [Docker SDK now marked as paid](https://discuss.huggingface.co/t/docker-sdk-now-marked-as-paid-when-creating-a-new-space/177580)
and [New free accounts cannot create CPU Basic Gradio Spaces](https://discuss.huggingface.co/t/new-free-accounts-cannot-create-cpu-basic-gradio-spaces-only-zerogpu-available/177629),
on the forum.

### June 2026: Gradio Requires huggingface_hub 1.x

**Change.** Gradio 6.18.0, released on June 11, raised its requirement on `huggingface-hub` from
0.33.5 to 1.2.0, and Gradio 6.21.0, released on July 29, raised it to 1.16.0. The last releases of
`transformers` 4.x require `huggingface-hub` below 1.0.

**Effect.** The ten Spaces whose models need `transformers` 4.x cannot move past Gradio 6.17.3. They
stay behind the rest of the fleet, which runs 6.24.0, and a future PyHARP that needs a newer Gradio
could not reach them until their code moves off `transformers` 4.x.

**Response.** They hold `sdk_version: 6.17.3`, the newest release in the window, as
[Pin Gradio Before Splitting the Environment](DEPLOYMENT.md#pin-gradio-before-splitting-the-environment)
describes.

**Sources.** The requirements each release of [gradio](https://pypi.org/project/gradio/#history) and
[transformers](https://pypi.org/project/transformers/#history) declares on PyPI.

### May 2026: ZeroGPU Moves to Blackwell GPUs

**Change.** By May 12, ZeroGPU had replaced its NVIDIA H200 GPUs with RTX PRO 6000 Blackwell GPUs,
with three consequences:

- ZeroGPU requires PyTorch 2.8.0 or newer, and accepts an exact `torch` pin only for the releases it
  lists. A PyTorch built for an older CUDA fails on a Blackwell GPU, and any other exact pin stops
  the Space before it starts, with `torch version in requirements.txt is not compatible with
  ZeroGPU`.
- The `spaces` client multiplies the `duration` of a call by 1.5 on a Blackwell GPU before ZeroGPU
  admits it, since version 0.49.1, released on May 11.
- A call gets 48 GB of GPU memory by default, less than it had on an H200.

**Effect.** Every ZeroGPU Space of the fleet still on an older PyTorch broke, and with it the model
in HARP for every user, until the Space was repinned. Repins went on Space by Space from June into
October. A request now needs half its declared duration in the allowance of the caller, so a
free account, with 5 minutes a day, cannot be admitted to a call declaring more than 200 s.

**Response.** Each Space moved to an accepted `torch`, with `torchaudio` to match, and reservations
were sized to each request. [Choosing the Hardware](DEPLOYMENT.md#choosing-the-hardware) lists the
accepted releases, which the lint checks (`zerogpu-torch`), and
[Protecting the Caller's Allowance](DEPLOYMENT.md#protecting-the-callers-allowance) describes the
factor.

**Sources.** [NVIDIA RTX PRO 6000 instead of H200 for ZeroGPU](https://discuss.huggingface.co/t/nvidia-rtx-pro-6000-instead-of-h200-for-zerogpu/175960)
on the forum, the [ZeroGPU documentation](https://huggingface.co/docs/hub/spaces-zerogpu), and
`zero/client.py` in [spaces](https://pypi.org/project/spaces/#history) 0.49.1.
