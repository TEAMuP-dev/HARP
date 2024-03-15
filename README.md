# HARP

A standalone application for HARP.

Check out the paper: https://neuripscreativityworkshop.github.io/2023/papers/ml4cd2023_paper23.pdf.

[![Netlify Status](https://api.netlify.com/api/v1/badges/d84e0881-13d6-49b6-b743-d176b175aa79/deploy-status)](https://app.netlify.com/sites/harp-plugin/deploys)

![herofig_revised](https://github.com/audacitorch/HARP/assets/26678616/c4f5cdbb-aaff-4196-b9d2-3b6f69130856)

TODO - update main figure with up-to-date screenshots

HARP is a sample editor that allows for **h**osted, **a**synchronous, **r**emote **p**rocessing of audio with machine learning. HARP works by routing audio through [Gradio](https://www.gradio.app) endpoints. Since Gradio applications can be hosted locally or in the cloud (e.g. with [HuggingFace Spaces](https://huggingface.co/spaces)), HARP lets users of Digital Audio Workstations (DAWs) capable of connecting with external sample editors (_e.g._ [Reaper](https://www.reaper.fm), Logic Pro X, or Ableton Live) access large state-of-the-art models using cloud-based services, without breaking the within-DAW workflow.

## OS and DAW compatibility
HARP has been tested on the following:
* MacOS (ARM) versions 13.0 and 13.4
* MacOS (x86) version 10.15
* Windows - TODO
* Linux - TODO

with [Reaper](https://www.reaper.fm) and Logic Pro X.

# Installing HARP
## MacOS
* Download the appropriate DMG file for HARP from the [releases](https://github.com/audacitorch/HARP/releases) page.

* Double click on the DMG file. This will open the window shown below.

TODO - add new DMG figure here

* Drag HARP.app to the `Applications/` folder to install HARP. 

## Windows
The windows build is still under development. See the `TODO` branch for updates on our progress.

## Linux
TODO

# HARP Guide
## REAPER
### Setting Up
* Choose _REAPER > Preferences_ on the file menu.

* Scroll down to _External Editors_ and click _Add_.

* Click _Browse_ to the right of the _Primary Editor_ field.

* Navigate to your `HARP.app` installation and select "OK".

TODO - add new setup figure here

### Using HARP
* Right click the audio for the track you want to process and select _Render items as new take_ to bounce the track.

* Right click the bounced audio and select _Open items in editor > Open items in 'HARP.app'_.

TODO - add external editor selection figure here

## Logic Pro X
### Setting Up
To use HARP in Logic Pro X, fist set up `HARP.app` as an external sample editor using this guide: https://support.apple.com/guide/logicpro/use-an-external-sample-editor-lgcp2158eb9a/mac. 

### Using HARP
Once HARP.app has been set up as an external editor, you can select any audio region and press Shift+W to open it correspnding audio file in HARP. Any changes you make in HARP will be automatically reflected in the DAW when you're done. 


## Ableton Live
### Setting Up
TODO
### Using HARP

# Using HARP as an External Sample Editor
After recording or loading audio into a track within your preferred DAW, it is recommended to bounce the track in order to avoid overwriting the original audio. If you would only like to process an excerpt of the track, trim the audio before performing the bounce.

**When using HARP, it is recommended that you "bounce-in-place" any audio regions you'd like to process with HARP before processing them. This gives you the chance to undo changes and revert to a backup of your original file.**


# HARP Usage
* After opening HARP as an external sample editor, the following window will appear.

<img width="799" alt="Screenshot 2024-03-14 at 11 15 07 AM" src="https://github.com/audacitorch/HARP/assets/55194054/5fe8a28b-a612-4b08-9857-fe3df84afa20">

* Type the [Gradio](https://www.gradio.app) endpoint of an available HARP-compatible model (_e.g._ "hugggof/harmonic_percussive") in the field with the text _path to a gradio endpoint_.

  * This will populate the window with controls for the model.

  * Loading may take some time if the [HuggingFace Space](https://huggingface.co/spaces) is asleep.

TODO - add new harmonic-percussive figure here

* Adjust the model controls to your liking and click _process_.

* The resulting audio can be played by pressing the space bar or by clicking _Play/Stop_.

# Available HARP-Compatible Models
While any algorithm or model can be made HARP-compatible with the [PyHARP API](https://github.com/audacitorch/pyharp), at present, the following are available for use within HARP:

* Pitch Shifting: [hugggof/pitch_shifter](https://huggingface.co/spaces/hugggof/pitch_shifter)

* Harmonic/Percussive Source Separation: [hugggof/harmonic_percussive](https://huggingface.co/spaces/hugggof/harmonic_percussive)

* Music Audio Generation: [descript/vampnet](https://huggingface.co/spaces/descript/vampnet)

* Convert Instrumental Music into 8-bit Chiptune: [hugggof/nesquik](https://huggingface.co/spaces/hugggof/nesquik)

* Music Audio Generation: [hugggof/MusicGen](https://huggingface.co/spaces/hugggof/MusicGen)

* Pitch-Preserving Timbre-Removal: [cwitkowitz/timbre-trap](https://huggingface.co/spaces/cwitkowitz/timbre-trap)

# Making Code HARP-Compatible
We also provide [PyHARP](https://github.com/audacitorch/pyharp), a lightweight API to build HARP-compatible [Gradio](https://www.gradio.app) apps with optional interactive controls. PyHARP allows machine learning researchers to create DAW-friendly user interfaces for virtually any audio processing code using a minimal Python wrapper.

# Building HARP
HARP can be built from scratch with the following steps:

**Clone the HARP repository**
```bash
git clone --recurse-submodules git@github.com:audacitorch/HARP.git
cd harp
```

## MacOS
**Configure**
```bash
mkdir build
cd build
cmake ..  -DCMAKE_BUILD_TYPE=Debug
```

**Build**
```bash
make -jNUM_PROCESSORS
```

### Building for ARM vs x86 MacOS
The OSX architecture for the build can be specified explicitly by setting  `CMAKE_OSX_ARCHITECTURES` to either `arm64` or `x86_64`:
```bash
cmake .. DCMAKE_OSX_ARCHITECTURES=x86_64
```

## Windows
HARP has been tested on Windows 10 x64. You can checkout the `windowsBuild` branch and follow the instructions in the README.

TODO - is this still valid?

# Codesigning and Distribution
## MacOS
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
bash packaging/package.sh <Developer ID Application> <Artifacts Path> <Project Name> <Product Name> <Notarization Username> <Notarization Password> <Team ID>
```

After running `package.sh`, you should have a signed and notarized dmg file in the `packaging/` directory.

## Windows
TODO

# Debugging a HARP Build
## Mac
1. Download [Visual Studio Code for macOS](https://code.visualstudio.com/).
2. Install the C/C++ extension from Microsoft.
3. Open the _Run and Debug_ tab in VS Code and click _create a launch.json file_ using LLDB.
4. Create a configuration to attach to the process (see the following example code to be placed in `launch.json`).

```
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "(lldb) Launch",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP.app",
            "args": ["./test.wav"], // TODO - remove?
            "stopAtEntry": false,
            "cwd": "${fileDirname}",
            "environment": [],
            "externalConsole": false,
            "MIMode": "lldb"
        }

    ]
}
```

5. Build the plugin using the flag `-DCMAKE_BUILD_TYPE=Debug`.
6. Add break points and run the debugger.

## Windows
TODO

## Linux
TODO - same as mac?
