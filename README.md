![herofig_revised](https://github.com/TEAMuP-dev/HARP/assets/26678616/c4f5cdbb-aaff-4196-b9d2-3b6f69130856)

![Build status](https://img.shields.io/github/actions/workflow/status/TEAMuP-dev/HARP/cmake_ctest.yml?branch=main)
[![Netlify Status](https://api.netlify.com/api/v1/badges/d84e0881-13d6-49b6-b743-d176b175aa79/deploy-status)](https://app.netlify.com/sites/harp-plugin/deploys)

HARP is a sample editor that allows for **h**osted, **a**synchronous, **r**emote **p**rocessing of audio with machine learning. HARP works by routing audio through [Gradio](https://www.gradio.app) endpoints. Since Gradio applications can be hosted locally or in the cloud (e.g. with [HuggingFace Spaces](https://huggingface.co/spaces)), HARP lets users of Digital Audio Workstations (DAWs) capable of connecting with external sample editors (_e.g._ [REAPER](https://www.reaper.fm), [Logic Pro X](https://www.apple.com/logic-pro/), or [Ableton Live](https://www.ableton.com/en/live/)) access large state-of-the-art models using cloud-based services, without breaking the within-DAW workflow.

For a more formal introduction, please see our [NeurIPS paper](https://neuripscreativityworkshop.github.io/2023/papers/ml4cd2023_paper23.pdf) presenting an earlier version of HARP.

## Table of Contents
* **[Installation](#installation)**
    * **[MacOS](#macos)**
    * **[Windows](#windows)**
    * **[Linux](#linux)**
* **[Setup](#setup)**
    * **[Standalone](#standalone)**
    * **[REAPER](#reaper)**
    * **[Logic Pro X](#logic-pro-x)**
    <!--* **[Ableton Live](#ableton-live)**-->
* **[Usage](#usage)**
    * **[Warning!](#warning)**
    * **[Models (PyHARP)](#models)**
    * **[Workflow](#workflow)**
* **[Contributing](#contributing)**
    * **[Building](#building)**
    * **[Debugging](#debugging)**
    * **[Distribution](#distribution)**
    * **[Citing](#citing)**



# Installation
HARP has been tested on the following operating systems:
| OS | ![macOS](https://img.shields.io/badge/mac%20os%20(ARM)-000000?style=for-the-badge&logo=macos&logoColor=F0F0F0) |  ![macOS](https://img.shields.io/badge/mac%20os%20(x86)-000000?style=for-the-badge&logo=macos&logoColor=F0F0F0) | ![Windows](https://img.shields.io/badge/Windows-0078D6?style=for-the-badge&logo=windows&logoColor=white) | ![Ubuntu](https://img.shields.io/badge/Ubuntu-E95420?style=for-the-badge&logo=ubuntu&logoColor=white) |
| :-: | :-: | :-: | :-: | :-: |
| Version(s) | 13.0, 13.4 | 10.15 | 10 | 22.04 |

## MacOS
* Download the macOS ZIP file for HARP from the [releases](https://github.com/TEAMuP-dev/HARP/releases) page.

* Unzip and double click on the DMG file. This will open the window shown below.

![DMG](https://github.com/TEAMuP-dev/HARP/assets/33099118/9ac4b75f-b4a1-4a3d-93fb-6b47c6821dd4)

* Drag `HARP.app` to the `Applications/` folder to install HARP.

## Windows
* Download the Windows ZIP file for HARP from the [releases](https://github.com/TEAMuP-dev/HARP/releases) page.

* Extract the contents of the ZIP file and move the directory containing `HARP.exe` to a location of your choice, _e.g._ `C:\Program Files`.

## Linux
* Download the Linux ZIP file for HARP from the [releases](https://github.com/TEAMuP-dev/HARP/releases) page.

* Extract the contents of the ZIP file and move the directory containing `HARP` to a location of your choice, _e.g._ `/usr/local/bin/`.



# Setup
## Standalone
### Opening HARP
#### MacOS
Run `HARP.app` to start the application.

#### Windows
Run `HARP.exe` to start the application.

#### Linux
Run `HARP` to start the application.

## [REAPER](https://www.reaper.fm)
### Setting Up HARP
* Choose _REAPER > Preferences_ on the file menu.

* Scroll down to _External Editors_ and click _Add_.

* Click _Browse_ to the right of the _Primary Editor_ field.

* Navigate to your HARP installation (*e.g.* `HARP.app`) and select "OK".

![setup](https://github.com/TEAMuP-dev/HARP/assets/33099118/cb233173-4aa4-45d8-9321-06d4f58daaa4)

### Opening HARP
* (Optional) Right click the audio for the track you want to process and select _Render items as new take_.

* Right click the audio and select _Open items in editor > Open items in 'HARP.app'_.

![external_editor](https://github.com/TEAMuP-dev/HARP/assets/33099118/6914fa60-06a0-42b2-98f1-1d8b7124de21)

## [Logic Pro X](https://www.apple.com/logic-pro/)
### Setting Up HARP
* Set `HARP.app` as an external sample editor following [this guide](https://support.apple.com/guide/logicpro/use-an-external-sample-editor-lgcp2158eb9a/mac).

### Opening HARP
* Select any audio region and press _Shift+W_ to open the corresponding audio file in HARP.

<!--
## [Ableton Live](https://www.ableton.com/en/live/)
### Setting Up HARP
TODO

### Opening HARP
TODO
-->



# Usage
## Warning!
**HARP is a *destructive* file editor.**
After recording or loading audio into a track within your preferred DAW, it is recommended that you *bounce-in-place* the audio before processing it with HARP. In this way, you will avoid overwriting the original audio file and will be able to undo any changes introuced by HARP. If you would only like to process an excerpt of the track, perform a *bounce-in-place* and trim the audio before opening it in HARP. Alternatively, overwriting can be circumvented by using the _Save As_ functionality from the _File_ menu.

## Models
### PyHARP API
[![ReadMe Card](https://github-readme-stats.vercel.app/api/pin/?username=TEAMuP-dev&repo=pyharp)](https://github.com/TEAMuP-dev/pyharp)

| HARP | PyHARP |
| :-: | :-: |
| 1.1.0 | 0.1.0 |

We provide PyHARP, a lightweight API to build HARP-compatible [Gradio](https://www.gradio.app) apps with optional interactive controls. PyHARP allows machine learning researchers to create DAW-friendly user interfaces for virtually any audio processing code using a minimal Python wrapper.

### Available Models
While any algorithm or model can be deployed to HARP using the PyHARP API, at present, the following have been made available:

* Pitch Shifting: [hugggof/pitch_shifter](https://huggingface.co/spaces/hugggof/pitch_shifter)

* Harmonic/Percussive Source Separation: [hugggof/harmonic_percussive](https://huggingface.co/spaces/hugggof/harmonic_percussive)

* Music Audio Generation: [descript/vampnet](https://huggingface.co/spaces/descript/vampnet)

* Convert Instrumental Music into 8-bit Chiptune: [hugggof/nesquik](https://huggingface.co/spaces/hugggof/nesquik)

<!--* Music Audio Generation: [hugggof/MusicGen](https://huggingface.co/spaces/hugggof/MusicGen)-->

* Pitch-Preserving Timbre-Removal: [cwitkowitz/timbre-trap](https://huggingface.co/spaces/cwitkowitz/timbre-trap)

## Workflow
* After opening HARP as an external sample editor or standalone application, the following window will appear.

<img width="799" alt="Screenshot 2024-03-14 at 11 15 07 AM" src="https://github.com/TEAMuP-dev/HARP/assets/55194054/5fe8a28b-a612-4b08-9857-fe3df84afa20">

* Select or type the [Gradio](https://www.gradio.app) endpoint of an available HARP-ready model (_e.g._ "hugggof/harmonic_percussive") in the field at the top of the application.

  * This will populate the window with controls for the model.

  * Loading can take some time if the [HuggingFace Space](https://huggingface.co/spaces) is asleep.

![harmonic_percussive](https://github.com/TEAMuP-dev/HARP/assets/33099118/20937933-01d1-402e-aab8-3253de0134c0)

* Adjust the model controls to your liking and click _process_.

* The resulting audio can be played by pressing the space bar or by clicking _Play/Stop_.

* You can save the audio at any time using the _Save_ and _Save As_ features located in the _File_ menu.

* Any changes made in HARP will be automatically reflected in your DAW.



# Contributing
## Building
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

## Debugging
### Visual Studio Code
1. Download [Visual Studio Code](https://code.visualstudio.com/).
2. Install the C/C++ extension from Microsoft.
3. Open the _Run and Debug_ tab in VS Code and click _create a launch.json file_ using _CMake Debugger_.
4. Create a configuration to attach to the process (see the following example code to be placed in `launch.json`).

```json5
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "(lldb) Standalone",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP.app", // macOS
            // "program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP.exe", // Windows
            // "program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP", // Linux
            "args": ["../test.wav"],
            "cwd": "${fileDirname}",
            "MIMode": "lldb" // macOS
        }
    ]
}
```

5. Build the plugin using the flag `-DCMAKE_BUILD_TYPE=Debug`.
6. Add break points and run the debugger.

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

<!--
### Windows
TODO
-->

## Citing
If you use HARP in your research, please cite our [NeurIPS paper](https://neuripscreativityworkshop.github.io/2023/papers/ml4cd2023_paper23.pdf):
```
@inproceedings{garcia2023harp,
    title     = {{HARP}: Bringing Deep Learning to the DAW with Hosted, Asynchronous, Remote Processing},
    author    = {Garcia, Hugo Flores and O’Reilly, Patrick and Aguilar, Aldo and Pardo, Bryan and Benetatos, Christodoulos and Duan, Zhiyao},
    year      = 2023,
    booktitle = {NeurIPS Workshop on Machine Learning for Creativity and Design}
}
```
