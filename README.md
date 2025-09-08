<!-- content/intro.md -->
# HARP

<!-- TODO - Update this figure! -->
![herofig_revised](https://github.com/TEAMuP-dev/HARP/assets/26678616/c4f5cdbb-aaff-4196-b9d2-3b6f69130856)

<!--![Build status](https://img.shields.io/github/actions/workflow/status/TEAMuP-dev/HARP/cmake_ctest.yml?branch=main) -->
<!--[![Netlify Status](https://api.netlify.com/api/v1/badges/d84e0881-13d6-49b6-b743-d176b175aa79/deploy-status)](https://app.netlify.com/sites/harp-plugin/deploys) -->
<!-- TODO - Replaced with HARP 3.0 paper link -->
<!--[![arXiv](https://img.shields.io/badge/arXiv-2503.02977-b31b1b.svg?style=flat)](https://arxiv.org/abs/2503.02977) -->

HARP is a sample editor for **h**osted, **a**synchronous, **r**emote **p**rocessing of audio with machine learning. HARP operates as a standalone application or a plugin-like editor within your DAW, and routes audio, MIDI, and metadata through [Gradio](https://www.gradio.app) endpoints for processing. Gradio apps can be hosted locally or remotely (_e.g._, [HuggingFace Spaces](https://huggingface.co/spaces)), allowing users of DAWs (_e.g._ [REAPER](https://www.reaper.fm), [Logic Pro X](https://www.apple.com/logic-pro/), [Ableton Live](https://www.ableton.com/en/live/)) to access powerful cloud-based models seamlessly from within their DAW.

## Table of Contents
* **[Installation](#installation)**
* **[Usage](#usage)**
    * **[Warning!](#warning)**
    * **[Models](#models)**
    * **[Workflow](#workflow)**
* **[Contributing](#contributing)**
    * **[Version Compatibility](#version-compatibility)**
    * **[Adding Models with PyHARP](#adding-models-with-pyharp)**
    * **[Building Harp](#building-harp)**
    * **[Debugging](#debugging)**
    * **[Distribution](#distribution)**
* **[Citations](#citations)**

<!-- TODO - add link to HARP 3.0 paper -->
For more information on HARP, please see [our website](https://harp-plugin.netlify.app), our [NeurIPS workshop paper](https://neuripscreativityworkshop.github.io/2023/papers/ml4cd2023_paper23.pdf), or our [ISMIR Late Breaking Demo](https://arxiv.org/abs/2503.02977).


<!-- content/supported_os.md -->
# Installation

HARP has been tested on the following operating systems:

| OS | ![macOS ARM](https://img.shields.io/badge/mac%20os%20(ARM)-000000?style=for-the-badge&logo=macos&logoColor=F0F0F0) | ![macOS x86](https://img.shields.io/badge/mac%20os%20(x86)-000000?style=for-the-badge&logo=macos&logoColor=F0F0F0) | ![Windows](https://img.shields.io/badge/Windows-0078D6?style=for-the-badge&logo=windows&logoColor=white) | ![Ubuntu](https://img.shields.io/badge/Ubuntu-E95420?style=for-the-badge&logo=ubuntu&logoColor=white) |
| :-: | :-: | :-: | :-: | :-: |
| Versions | 13.0, 13.4, 14.2.1, 14.5 | 10.15 | 10, 11 | 22.04 |

<!-- content/install/ -->
<!-- content/setup/ -->
Please visit [our website](https://harp-plugin.netlify.app/content/install/macos.html) for instructions on installing HARP for different operating systems and setting it up as an external sample editor for different DAWs.


# Usage

HARP can be used to apply deep learning models to your audio either as a stand-alone or within any DAW (e.g. Logic Pro) that supports external sample editors.

If you use it stand-alone, just load a file, load a model and apply the model to the audio file. When you're happy with the result, save the output.

In a DAW, you select the exceprt you want to process, open it in HARP, process it, and select _Save_ from the  _File_ menu in HARP. This will return the processed file back to the DAW.

<!-- content/usage/warnings.md -->
## Warning!

**HARP is a _destructive_ file editor.** When operating HARP as a standalone application, use _Save As_ to avoid overwriting input files. If you save your outputs while operating in the DAW, HARP will overwrite input regions. By creating a duplicate / bounce / alternate take of the region you want to edit with HARP, you can ensure the original region remains unaffected.

<!-- content/usage/partial_track.md -->
### Processing just a portion of a track

HARP processes full regions in the DAW. Therefore, to edit a portion of an audio or MIDI region:

- Split the region to obtain the excerpt you want to edit as a separate region
- (Optional) Create a duplicate / bounce / alternate take of the region you want to edit
- Open with HARP to process

<!-- content/usage/models.md -->
## Models

Please visit [our website](https://harp-plugin.netlify.app/content/usage/models.html) for a full list of supported models.

<!-- content/usage/workflow.md -->
## Workflow

HARP supports a simple workflow: pick an existing model for processing or provide a URL to your own, load the model and it's corresponding interface, tweak controls to your liking, and process.

To get started:

- Open HARP as a standalone application or within your DAW
- Select an existing model using the drop-down menu at the top of the screen, or select `custom path...` and provide a URL to any HARP-compatible Gradio endpoint 
- Load the selected model (and its corresponding interface) using the `Load` button.
- Import audio or MIDI data to process with the model either via the `Open File` button or by dragging and dropping a file into HARP
- Adjust controls to taste in the interface
- Click `Process` to run the model; outputs will automatically be rendered in HARP
- To save your outputs, click the `Save` button or select `Save As` from the `File` menu

<img width="1819" height="1042" alt="text-to-audio" src="https://github.com/user-attachments/assets/a5579d82-3955-46a1-84a6-22f7632a9d51" />


<!-- content/contributing/overview.md -->
# Contributing

To get started building and deploying models for others to use in HARP, see [Adding Models with PyHARP](#adding-models-with-pyharp). To get started developing the HARP app itself, see [Building Harp](#building-harp). 

<!-- content/contributing/version_compat.md -->
## Version Compatibility

The currently available versions of HARP and PyHARP are mutually compatible.

| HARP | PyHARP |
| :-: | :-: |
| 3.0.0 | 0.3.0 |
| 2.2.0 | 0.2.1 |

<!-- content/contributing/add_model.md -->
# Adding Models with PyHARP

[![ReadMe Card](https://github-readme-stats.vercel.app/api/pin/?username=TEAMuP-dev&repo=pyharp)](https://github.com/TEAMuP-dev/pyharp)

PyHARP provides a lightweight API to build HARP-compatible [Gradio](https://www.gradio.app) apps. It allows researchers to easily create DAW-friendly interfaces for any audio processing code with minimal Python wrappers.

<!-- content/contributing/build_source.md -->
# Building HARP

HARP can be built from scratch with the following steps:

### 1. Clone Repository

```bash
git clone --recurse-submodules https://github.com/TEAMuP-dev/HARP
```

### 2. Enter Project

```bash
cd HARP/
```

### 3. Configure

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

#### ARM vs. x86 MacOS

The OSX architecture for the build can be specified explicitly by setting `CMAKE_OSX_ARCHITECTURES` to either `arm64` or `x86_64`:
```bash
cmake .. -DCMAKE_OSX_ARCHITECTURES=x86_64
```

#### Linux

Ensure your system satisfies all [JUCE dependencies](https://github.com/juce-framework/JUCE/blob/master/docs/Linux%20Dependencies.md).

### 4. Build
#### MacOS/Linux

```bash
make -j <NUM_PROCESSORS>
```

#### Windows

```bash
cmake --build . --config Debug -j <NUM_PROCESSORS>
```

<!-- content/contributing/debug.md -->
## Debugging

We provide instructions for debugging your HARP build in [Visual Studio Code](https://code.visualstudio.com/):

1. Download [Visual Studio Code](https://code.visualstudio.com/Download).
2. Install the C/C++ extension from Microsoft.
3. Open the _Run and Debug_ tab in VS Code and click _create a launch.json file_ using _CMake Debugger_.
4. Create a configuration to attach to the process (see the following example code to be placed in `launch.json`).

```json5
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Standalone HARP",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP.app", // macOS
            //"program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP.exe", // Windows
            //"program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP", // Linux
            "args": ["../test/test.wav", "../test/test.mid"],
            "cwd": "${fileDirname}",
            "MIMode": "lldb" // macOS
        },
        {
            "name": "Attach to HARP",
            "type": "cppdbg",
            "request": "attach",
            "program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP.app", // macOS
            //"program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP.exe", // Windows
            //"program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP", // Linux
            "processId": "${command:pickProcess}",
            "MIMode": "lldb" // macOS
        }
    ]
}
```

5. Build the plugin using the flag `-DCMAKE_BUILD_TYPE=Debug` (see the following configure / build commands for macOS).

```bash
# CMake Configure Command:
/opt/homebrew/bin/cmake -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/clang -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/clang++ --no-warn-unused-cli -S /Users/<USER>/Projects/HARP -B /Users/<USER>/Projects/HARP/build -G Ninja
```

```bash
# CMake Build Command:
/opt/homebrew/bin/cmake --build /Users/<USER>/Projects/HARP/build --config Debug --target all --
```

6. Add break points and run the debugger.

<!-- content/contributing/dist.md -->
## Distribution

### MacOS

Codesigning and packaging for distribution is done through the script located at `packaging/package.sh`.
You'll need to set up a developer account with Apple and create a certificate in order to sign the plugin.
For more information on codesigning and notarization for macOS, please refer to the [pamplejuce](https://github.com/sudara/pamplejuce) template.

The script requires the following variables to be passed:
```
# Retrieve values from either environment variables or command-line arguments
DEV_ID_APPLICATION # Developer ID Application certificate
ARTIFACTS_PATH # should be packaging/dmg/HARP.app
PROJECT_NAME # "HARP"
PRODUCT_NAME # "HARP"
NOTARIZATION_USERNAME # Apple ID
NOTARIZATION_PASSWORD # App-specific password for notarization
TEAM_ID # Team ID for notarization
```

Usage:
```bash
./HARP/packaging/package.sh <DEV_ID_APPLICATION> <ARTIFACTS_PATH> <PROJECT_NAME> <PRODUCT_NAME> <NOTARIZATION_USERNAME> <NOTARIZATION_PASSWORD> <TEAM_ID>
```

After running `package.sh`, you should have a signed and notarized dmg file in the `packaging/` directory.

## Citations

<!-- TODO - HARP 3.0 paper citation -->

[ISMIR 2024 Late Breaking Demo](https://ismir2024program.ismir.net/lbd_497.html):
```
@article{benetatos2024harp,
    title     = {{HARP} 2.0: Expanding Hosted, Asynchronous, Remote Processing for Deep Learning in the {DAW}},
    author    = {Benetatos, Christodoulos and Cwitkowitz, Frank and Pruyne, Nathan and Garcia, Hugo Flores and O'Reilly, Patrick and Duan, Zhiyao and Pardo, Bryan},
    year      = 2024,
    journal   = {ISMIR Late Breaking Demo Papers}
}
```

[NeurIPS 2023 Paper](https://neuripscreativityworkshop.github.io/2023/papers/ml4cd2023_paper23.pdf):
```
@inproceedings{garcia2023harp,
    title     = {{HARP}: Bringing Deep Learning to the {DAW} with Hosted, Asynchronous, Remote Processing},
    author    = {Garcia, Hugo Flores and O’Reilly, Patrick and Aguilar, Aldo and Pardo, Bryan and Benetatos, Christodoulos and Duan, Zhiyao},
    year      = 2023,
    booktitle = {NeurIPS Workshop on Machine Learning for Creativity and Design}
}
```
