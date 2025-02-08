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
    * **[Models](#models)**
    * **[Workflow](#workflow)**
* **[Contributing](#contributing)**
    * **[Version Compatibility](#version-compatibility)**
    * **[Adding Models with PyHARP](#adding-models-with-pyharp)**
    * **[Building Harp](#building-harp)**
    * **[Debugging](#debugging)**
    * **[Distribution](#distribution)**
* **[Citing](#citing)**



# Installation
HARP has been tested on the following operating systems:
| OS | ![macOS](https://img.shields.io/badge/mac%20os%20(ARM)-000000?style=for-the-badge&logo=macos&logoColor=F0F0F0) |  ![macOS](https://img.shields.io/badge/mac%20os%20(x86)-000000?style=for-the-badge&logo=macos&logoColor=F0F0F0) | ![Windows](https://img.shields.io/badge/Windows-0078D6?style=for-the-badge&logo=windows&logoColor=white) | ![Ubuntu](https://img.shields.io/badge/Ubuntu-E95420?style=for-the-badge&logo=ubuntu&logoColor=white) |
| :-: | :-: | :-: | :-: | :-: |
| Version(s) | 13.0, 13.4, 14.2.1, 14.5 | 10.15 | 10, 11 | 22.04 |

## MacOS
* Download the macOS ZIP file for HARP from the [releases](https://github.com/TEAMuP-dev/HARP/releases) page.

* Unzip and double click on the DMG file. This will open the window shown below.
<p align="center">
   <img width="528" alt="HARP_DMG" src="https://github.com/TEAMuP-dev/HARP/assets/33099118/3f0278b2-8ace-41a1-8383-9c4aed894d56">
</p>

* Drag `HARP.app` to the `Applications/` folder to install HARP.

* Do NOT run  harp from the installation window above. It will not run correctly if you do so.

## Windows
* Download the Windows ZIP file for HARP from the [releases](https://github.com/TEAMuP-dev/HARP/releases) page.

* Extract the contents of the ZIP file and move the directory containing `HARP.exe` to a location of your choice, _e.g._ `C:\Program Files`.

## Linux
* Download the Linux ZIP file for HARP from the [releases](https://github.com/TEAMuP-dev/HARP/releases) page.

* Extract the contents of the ZIP file and move the directory containing `HARP` to a location of your choice, _e.g._ `/usr/local/bin/`.


# Setup
## Standalone
To work as a standalone application, you just need to open HARP and start using it.    
### Opening HARP
#### MacOS
Run `HARP.app` to start the application.

#### Windows
Run `HARP.exe` to start the application.

#### Linux
Run `HARP` to start the application.

## [REAPER](https://www.reaper.fm)
To set up HARP for use from within Reaper, do the following.
### Setting Up HARP
* Choose _REAPER > Preferences_ on the file menu for MacOS. OR hit keyboard command *control+p* on Windows

* Scroll down to _External Editors_ and click _Add_.

* Click _Browse_ to the right of the _Primary Editor_ field.

* Navigate to your HARP installation (*e.g.* `HARP.app`) and select "OK".
<p align="center">
   <img width="1023" alt="REAPER_Setup" src="https://github.com/TEAMuP-dev/HARP/assets/33099118/b828a2fd-5378-490c-be37-11f7404eb127">
</p>

### Opening HARP
* (Optional) Right click the audio for the track you want to process and select _Render items as new take_.

* Right click the audio and select _Open items in editor > Open items in 'HARP.app'_.
<p align="center">
   <img width="1531" alt="REAPER_Opening" src="https://github.com/TEAMuP-dev/HARP/assets/33099118/7f26857f-61de-4765-9671-fb69c4264dc4">
</p>

## [Logic Pro X](https://www.apple.com/logic-pro/)
To set up HARP for use from within Logic Pro, do the following.

### Setting Up HARP
* Set `HARP.app` as an external sample editor following [this guide](https://support.apple.com/guide/logicpro/use-an-external-sample-editor-lgcp2158eb9a/mac).

### Opening HARP

* (Optional) Right click the audio for the track you want to process and select _Bounce in Place_.

* Select any audio region and press _Shift+W_ to open the corresponding audio file in HARP.

<!--
## [Ableton Live](https://www.ableton.com/en/live/)
### Setting Up HARP
TODO

### Opening HARP
TODO
-->

## [Acoustica Mixcraft](https://acoustica.com/products/mixcraft)
To set up HARP for use from within Mixcraft, do the following.

### Setting Up HARP
* Set `HARP.exe` as an external wave editor following "Configuring an External Wave Editor" on [this manual page](https://acoustica.com/mixcraft-10-manual/audio-clips).

### Opening HARP

* Right click the audio for the track you want to process and select _Edit In External Editor..._, or via _Sound > Edit In External Editor..._.

* On the dialog that appears, choose _Edit A Copy Of The Sound_, and click _Edit_ to open HARP.

* After editing, close HARP and select _Done_ in the Mixcraft dialog box to update the track.

# Usage
HARP can be used to apply deep learning models to your audio either as a stand-alone or within any DAW (e.g. Logic Pro) that supports external sample editors.

If you use it stand-alone, just load a file, load a model and apply the model to the audio file. When you're happy with the result, save the output.

In a DAW, you select the exceprt you want to process, open it in HARP, process it, and select _Save_ from the  _File_ menu in HARP. This will return the processed file back to the DAW.

## Warning!
**HARP is a *destructive* file editor.**
When you select _Save_, HARP overwrites the existing audio. After recording or loading audio into a track within your preferred DAW, it is recommended that you *bounce-in-place* (on Logic) or _Render items as new take_ (on Reaper) the audio before processing it with HARP. In this way, you will avoid overwriting the original audio file and will be able to undo any changes introuced by HARP. Alternatively, overwriting can be circumvented by using the _Save As_ functionality from the _File_ menu in HARP.

### Processing just a portion of a track
If you would like to process only an excerpt of a track, first trim the exceprt to the portion you want to process. Then, perform a *bounce-in-place* of the excerpt. This will make a new file that contains only the audio you want to process with HARP. Then, open the new file in HARP. 

## Models

While any algorithm or deep learning model can be deployed to HARP using the PyHARP API, at present, the following models have been made available:

* Pitch Shifting: [hugggof/pitch_shifter](https://huggingface.co/spaces/hugggof/pitch_shifter)

* Harmonic/Percussive Source Separation: [hugggof/harmonic_percussive](https://huggingface.co/spaces/hugggof/harmonic_percussive)

* Music Audio Generation: [descript/vampnet](https://huggingface.co/spaces/descript/vampnet)

* Convert Instrumental Music into 8-bit Chiptune: [hugggof/nesquik](https://huggingface.co/spaces/hugggof/nesquik)

<!--* Music Audio Generation: [hugggof/MusicGen](https://huggingface.co/spaces/hugggof/MusicGen)-->

* Pitch-Preserving Timbre-Removal: [cwitkowitz/timbre-trap](https://huggingface.co/spaces/cwitkowitz/timbre-trap)

## Workflow
* After opening HARP as an external sample editor or standalone application, the following window will appear.

<!-- <img width="799" alt="Screenshot 2024-03-14 at 11 15 07 AM" src="https://github.com/TEAMuP-dev/HARP/assets/55194054/5fe8a28b-a612-4b08-9857-fe3df84afa20"> -->
<!-- <img width="798" alt="Screenshot 2024-06-04 at 3 11 35 AM" src="https://github.com/TEAMuP-dev/HARP/assets/15819935/35f22685-d4f3-451b-9051-321d139ffd59"> -->
<img width="791" alt="Screenshot 2024-06-04 at 4 29 03 AM (1)" src="https://github.com/TEAMuP-dev/HARP/assets/15819935/fa1d100c-519b-4eff-9296-c2900d05005c">

* Select or type the [Gradio](https://www.gradio.app) endpoint of an available HARP-ready model (_e.g._ "hugggof/harmonic_percussive") in the field at the top of the application.

  * This will populate the window with controls for the model.

  * Loading can take some time if the [HuggingFace Space](https://huggingface.co/spaces) is asleep.

<!-- ![harmonic_percussive](https://github.com/TEAMuP-dev/HARP/assets/33099118/20937933-01d1-402e-aab8-3253de0134c0) -->
<!-- <img width="797" alt="Screenshot 2024-06-04 at 3 12 31 AM" src="https://github.com/TEAMuP-dev/HARP/assets/15819935/eeb7be3e-f6a4-4d07-a8dc-33f8bf5fcb77"> -->
<img width="787" alt="Screenshot 2024-06-04 at 4 31 05 AM" src="https://github.com/TEAMuP-dev/HARP/assets/15819935/79dcaaa8-fa55-4ae9-b37b-7924fffd2331">

* Adjust the model controls to your liking and click _process_.

* The resulting audio can be played by pressing the space bar or by clicking _Play/Stop_.

* You can save the audio at any time using the _Save_ and _Save As_ features located in the _File_ menu.

* Any changes made in HARP will be automatically reflected in your DAW.



# Contributing
To make a model that you can use in HARP, see [Adding Models with PyHARP](#adding-models-with-pyharp). To modify the HARP app, see [Building Harp](#building-harp). 

## Version Compatibility
The current versions of HARP and PyHARP are shown below. They are fully compatible.

| HARP | PyHARP |
| :-: | :-: |
| 2.1.0 | 0.2.1 |
| 2.0.0 | 0.2.0 |
| 1.3.0 | 0.1.1 |
| 1.2.0 | 0.1.0 |

## Adding Models with PyHARP
[![ReadMe Card](https://github-readme-stats.vercel.app/api/pin/?username=TEAMuP-dev&repo=pyharp)](https://github.com/TEAMuP-dev/pyharp)

We provide PyHARP, a lightweight API to build HARP-compatible [Gradio](https://www.gradio.app) apps with optional interactive controls. PyHARP allows machine learning researchers to create DAW-friendly user interfaces for virtually any audio processing code using a minimal Python wrapper.


## Building HARP
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
To debug your HARP build in [Visual Studio Code](https://code.visualstudio.com/) it is helpful to do the following. 
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
