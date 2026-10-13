<!-- website/content/intro.md -->
# HARP

<!-- TODO: hero figure. A DAW session with HARP open over a selected region, mid
     process, so that the relationship between the two is clear at a glance. The
     figure this replaces predates HARP 3.0 and shows an interface that no longer
     exists. -->

<!--![Build status](https://img.shields.io/github/actions/workflow/status/TEAMuP-dev/HARP/build.yml?branch=main) -->
<!--[![Netlify Status](https://api.netlify.com/api/v1/badges/d84e0881-13d6-49b6-b743-d176b175aa79/deploy-status)](https://app.netlify.com/sites/harp3/deploys) -->
<!-- TODO: arXiv badge, once the HARP 3.0 paper has a preprint to point at.
<!--[![arXiv](https://img.shields.io/badge/arXiv-2503.02977-b31b1b.svg?style=flat)](https://arxiv.org/abs/2503.02977) -->

HARP is a sample editor for **h**osted, **a**synchronous, **r**emote **p**rocessing of audio with
machine learning. It runs as a standalone application or as a plugin-like editor within a DAW, and
sends audio, MIDI, and metadata to [Gradio](https://www.gradio.app) endpoints for processing.

Most machine learning models for audio are impractical to run alongside a session. They expect a
GPU, a particular version of PyTorch, and weights measured in gigabytes, and they take far longer
than a buffer period to produce a result. HARP leaves each model where it is already installed,
whether that is a [Hugging Face Space](https://huggingface.co/spaces) or a machine on your own
network, and returns the result to the editor. Processing is asynchronous and happens outside the
audio thread, so a model that runs for a minute delays nothing but itself.

This gives users of DAWs that support external sample editors, among them
[REAPER](https://www.reaper.fm), [Logic Pro](https://www.apple.com/logic-pro/), and
[Ableton Live](https://www.ableton.com/en/live/), access to these models without leaving the
session.

## Table of Contents

* **[Installation](#installation)**
* **[Usage](#usage)**
* **[Contributing](#contributing)**
* **[Citations](#citations)**

Further documentation is on [our website](https://harp3.netlify.app/).

<!-- website/content/supported_os.md -->
<!-- website/content/install/ -->
<!-- website/content/setup/ -->
# Installation

Download a package from the [latest release](https://github.com/TEAMuP-dev/HARP/releases). Every
push is built for each platform below.

| Platform | Package | Requires |
| :-- | :-- | :-- |
| macOS | Universal binary, signed and notarized | macOS 11 or newer, on Apple silicon or Intel |
| Windows | x64 executable | Windows 10 or 11 |
| Linux | x64 and arm64 archives | glibc 2.35 or newer |

The Linux packages are built on the oldest supported Ubuntu, which sets that glibc requirement. They
run on Ubuntu 22.04, Debian 12, Fedora 36, RHEL 10, openSUSE Leap 16, and Arch, or anything newer.
At runtime HARP needs ALSA, FreeType, and X11, and loads libcurl for network requests, all of
which a desktop installation provides. Under Wayland it runs through XWayland, which GNOME and
KDE enable by default.

Note that the Windows executable is not code signed, so Windows will warn before running it the first
time.

Our website has [per-platform instructions](https://harp3.netlify.app/content/install/macos.html)
for installing HARP and configuring it as an external sample editor in a particular DAW.

# Usage

As a Standalone, open a file, open a model, process, and save the result. Within a DAW, select
the region to process, open it in HARP, and either return the result with the _Send-to-DAW_ icon or
write it to disk with the _Save_ icon.

<!-- website/content/usage/warnings.md -->
## Warning

**HARP is a _destructive_ file editor.** When running it as a standalone, use _Save As_
to avoid overwriting the input. _Send-to-DAW_ overwrites the region the audio came from. Working on
a duplicate, a bounce, or an alternate take leaves the original intact.

<!-- website/content/usage/partial_track.md -->
HARP processes whole regions, so editing part of one means splitting it first, to obtain the excerpt
as a region of its own.

<!-- website/content/usage/workflow.md -->
## Workflow

- Open HARP, either standalone or from a DAW.
- On the `Home` tab, browse or search the models hosted by
  [TEAMuP on Hugging Face](https://huggingface.co/teamup-tech), which are grouped by function,
  or choose `Custom Path...` and supply the URL of any HARP-compatible Gradio endpoint.
- Select a model to open it in a tab of its own. Several models can be open at once.
- Load audio or MIDI with `Open File`, or by dragging a file onto the track.
- Set the controls, then select `Process`. Results are rendered in place as they arrive.
- Save the result with the `Save` icon, or return it to the DAW with `Send-to-DAW`.

Media can also be moved around without going through the filesystem. The media clipboard holds
tracks independently of any one model, so a result from one model can be carried into another as its
input. Files can be dragged into HARP from elsewhere, and tracks can be dragged or copied and pasted
out of HARP into a DAW or a file browser.

<!-- TODO: screenshot of a model tab mid-process, showing the controls, an input track, a rendered
     output track, and the status area reporting progress. The figure this replaces predates the
     tabbed interface. -->

<!-- TODO: screenshot of the media clipboard with two tracks in it, one being dragged onto a model's
     input track, which is the part of the workflow hardest to describe in prose. -->

<!-- website/content/usage/models.md -->
## Models

Our website lists [every model we host](https://harp3.netlify.app/content/usage/models.html).

Each model is deployed under its original license, which HARP displays on a chip of its own beside
the model, so that the terms are apparent before the output is used. Authors who would rather their
model were not featured, or whose license we have stated incorrectly, are asked to
[open an issue](https://github.com/TEAMuP-dev/HARP/issues) so that we can correct or remove it
promptly.

<!-- website/content/contributing/overview.md -->
# Contributing

There are two ways to contribute, depending on what you intend to build.

**To deploy a model for others to use in HARP**, begin with [pyharp](https://github.com/TEAMuP-dev/pyharp),
which wraps arbitrary Python code in a HARP-compatible Gradio app. Its README covers deployment, and our
[deployment guidelines](https://github.com/TEAMuP-dev/HARP/blob/main/docs/DEPLOYMENT.md) describe
the conventions our own models follow: licensing, naming and tagging, sourcing weights, reporting
errors, preserving the input format, and choosing hardware.

[![PyHARP](https://gh-card.dev/repos/TEAMuP-dev/pyharp.svg)](https://github.com/TEAMuP-dev/pyharp)

**To work on the application itself**, see [Building HARP](#building-harp) below, together with our
guidelines for [developing](https://github.com/TEAMuP-dev/HARP/blob/main/docs/DEVELOPING.md),
[reviewing](https://github.com/TEAMuP-dev/HARP/blob/main/docs/REVIEWING.md), and
[code signing](https://github.com/TEAMuP-dev/HARP/blob/main/docs/SIGNING.md).

<!-- website/content/contributing/version_compat.md -->
HARP and pyharp are released in mutually compatible pairs: HARP 4.0 with pyharp 0.4.1, HARP 3.1.0
with pyharp 0.3.0, and HARP 2.2.0 with pyharp 0.2.1. The root `VERSION` file sets the application
version through CMake.

<!-- website/content/contributing/build_source.md -->
## Building HARP

The same commands build HARP on macOS, Windows, and Linux:

```bash
git clone --recurse-submodules https://github.com/TEAMuP-dev/HARP
cd HARP
cmake -B build -DCMAKE_BUILD_TYPE=Debug .
cmake --build build --config Debug --parallel
```

The binary lands in `build/HARP_artefacts/Debug/`, as `HARP.app` on macOS, `HARP.exe` on Windows,
and `HARP` on Linux. It accepts media files as arguments, and can be invoked from the command line:

```bash
./build/HARP_artefacts/Debug/HARP resources/media/test.wav resources/media/test.mid
```

Two platforms require additional steps. On macOS, `-DCMAKE_OSX_ARCHITECTURES` set to `arm64`, `x86_64`,
or `"arm64;x86_64"` selects the architecture, the last of these producing a universal binary. On
Linux, the
[JUCE dependencies](https://github.com/juce-framework/JUCE/blob/master/docs/Linux%20Dependencies.md)
must be installed first. There are no C++ unit tests, so a change is verified by running the
application.

<!-- website/content/contributing/debug.md -->
## Debugging

In [Visual Studio Code](https://code.visualstudio.com/), install Microsoft's C/C++ extension, then
add a configuration to `launch.json` pointing at the built binary, uncommenting the line for the
platform in use:

```json5
{
    "name": "Standalone HARP",
    "type": "cppdbg",
    "request": "launch",
    "program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP.app", // macOS
    //"program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP.exe", // Windows
    //"program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP", // Linux
    "args": ["../resources/media/test.wav", "../resources/media/test.mid"],
    "cwd": "${fileDirname}",
    "MIMode": "lldb" // macOS
}
```

An alternative configuration using `"request": "attach"` with `"processId": "${command:pickProcess}"`
attaches to an instance of HARP that is already running. This can be used to debug HARP after launching it from a DAW.

<!-- website/content/contributing/dist.md -->
## Distribution

[GitHub Actions](https://github.com/TEAMuP-dev/HARP/blob/main/.github/workflows/build.yml) builds
and packages every push. Pushing a tag of the form `v*` also publishes a release, taking
`RELEASE.md` as its notes.

A macOS build can be packaged locally into a DMG with
[dmgbuild](https://github.com/dmgbuild/dmgbuild):

```bash
packaging/package.sh build/HARP_artefacts/Release/HARP.app HARP.dmg
```

Signing and notarizing that DMG in CI requires the `MACOS_CERTIFICATE` and
`MACOS_CERTIFICATE_PASSWORD` secrets for the signing certificate, and `APPLE_ID` with
`APPLE_APP_PASSWORD` for notarization.
[SIGNING.md](https://github.com/TEAMuP-dev/HARP/blob/main/docs/SIGNING.md) covers both, along with
signing a build on your own machine.

## Citations

[NeurIPS 2025 Workshop on AI for Music Paper](https://openreview.net/pdf?id=Ha2z921r51):
```
@inproceedings{cwitkowitz2025harp,
    title     = {{HARP} 3.0: Generalizing {I/O} and {API} Support for Machine Learning in Digital Audio Workstations},
    author    = {Cwitkowitz, Frank and Benetatos, Christodoulos and Deng, Qixin and Yu, Huiran and Pruyne, Nathan and O'Reilly, Patrick and Garcia, Hugo Flores and Duan, Zhiyao and Pardo, Bryan},
    year      = 2025,
    booktitle = {NeurIPS AI for Music Workshop}
}
```

[ISMIR 2024 Late Breaking Demo](https://ismir2024program.ismir.net/lbd_497.html):
```
@article{benetatos2024harp,
    title     = {{HARP} 2.0: Expanding Hosted, Asynchronous, Remote Processing for Deep Learning in the {DAW}},
    author    = {Benetatos, Christodoulos and Cwitkowitz, Frank and Pruyne, Nathan and Garcia, Hugo Flores and O'Reilly, Patrick and Duan, Zhiyao and Pardo, Bryan},
    year      = 2024,
    journal   = {ISMIR Late Breaking Demo Papers}
}
```

[NeurIPS 2023 Workshop on Machine Learning for Creativity and Design Paper](https://neuripscreativityworkshop.github.io/2023/papers/ml4cd2023_paper23.pdf):
```
@inproceedings{garcia2023harp,
    title     = {{HARP}: Bringing Deep Learning to the {DAW} with Hosted, Asynchronous, Remote Processing},
    author    = {Garcia, Hugo Flores and O’Reilly, Patrick and Aguilar, Aldo and Pardo, Bryan and Benetatos, Christodoulos and Duan, Zhiyao},
    year      = 2023,
    booktitle = {NeurIPS Workshop on Machine Learning for Creativity and Design}
}
```
