# HARP

check out the paper: https://neuripscreativityworkshop.github.io/2023/papers/ml4cd2023_paper23.pdf

[![Netlify Status](https://api.netlify.com/api/v1/badges/d84e0881-13d6-49b6-b743-d176b175aa79/deploy-status)](https://app.netlify.com/sites/harp-plugin/deploys)

![herofig_revised](https://github.com/audacitorch/HARP/assets/26678616/c4f5cdbb-aaff-4196-b9d2-3b6f69130856)

HARP is an [ARA](https://www.synchroarts.com/blog/what-is-ara) plug-in that allows for **h**osted, **a**synchronous, **r**emote **p**rocessing of audio with deep learning models. HARP works by routing audio from a digital audio workstation ([DAW](https://en.wikipedia.org/wiki/Digital_audio_workstation)) through [Gradio](https://www.gradio.app) endpoints. Because Gradio apps can be hosted locally or in the cloud (e.g., HuggingFace Spaces), HARP lets users of Digital Audio Workstations (e.g. [Reaper](https://www.reaper.fm)) access large state-of-the-art models in the cloud, without breaking their within-DAW workflow.

# OS and DAW compatibility
HARP has been tested on arm-based Mac computers running Mac OS (versions 13.0 and 13.4), using the [REAPER](https://www.reaper.fm) digital audio workstation.

HARP requires a [DAW](https://en.wikipedia.org/wiki/Digital_audio_workstation) that fully supports the [Audio Random Access](https://en.wikipedia.org/wiki/Audio_Random_Access) to [VST](https://en.wikipedia.org/wiki/Virtual_Studio_Technology) plugins.

# Installing HARP
## MacOS
* Download the HARP DMG file from from the HARP [releases](https://github.com/audacitorch/HARP/releases)

* Double click on the DMG file. This will open the window below
<img width="397" alt="harp_dmg" src="https://github.com/audacitorch/HARP/assets/26678616/61acf9f3-8e00-4b85-9433-77366b262e19">

* Double click on "Your Mac's VST3 folder"

* Drag HARP.vst3 to the folder that was opened in the previous step

## Windows & Linux
The windows build is still under development. There are no current plans to support Linux

# Quickstart: Using HARP in the Reaper DAW on MacOS 13
## Making HARP available in Reaper

* Install the latest [Reaper](https://www.reaper.fm)

* Install HARP (see above)

* Start Reaper

* Open the preferences dialog by selecting the Reaper>Settings menu item

* Scroll down to find Plug-ins>ARA and make sure "Enable ARA for plug-ins" is checked.
  <img width="720" alt="reaper-preferences" src="https://github.com/audacitorch/HARP/assets/26678616/4fc157b1-4718-4da6-a395-37293af7e358">

* Restart Reaper

HARP should now be available as a VST3 plugin.

## Apply a HARP effect

* Record a track in Reaper

* Select "FX" on the track's channel strip. This brings up the following dialog
<img width="1196" alt="selecting-HARP" src="https://github.com/audacitorch/HARP/assets/26678616/4df7fdcd-582e-4e21-905e-fd06f374d0bf">

* Add HARP(TeamUP) as a VST3 plugin. This will call up HARP
<img width="1041" alt="harp-basic" src="https://github.com/audacitorch/HARP/assets/26678616/4794e2c9-fc97-4c31-bb63-bba3cadd5d1f">

* Type the gradio endpoint of an available HARP mode where it says "path to a gradio endpoint." For example "hugggof/harmonic_percussive". This will bring up the model controls.
<img width="1038" alt="harmonic_percussive" src="https://github.com/audacitorch/HARP/assets/26678616/44ee6ab1-e020-45d6-a87c-1ace3e76aefd">

* Adjust the controls and hit "process"

* To hear your result, just hit the space bar.

Warning: please note that Reaper may block the use of hotkeys (i.e. CTRL + {A, C, V}) and the space bar within text fields by default. However, these can be enabled by checking "Send all keyboard input to plug-in" under the "FX" window.

# Available Models
While any model can be made HARP-compatible with the [pyHARP API](https://github.com/audacitorch/pyharp), at present, the following models are available for use within HARP. Just enter the gradio path (e.g. "hugggof/pitch_shifter" or "descript/vampnet") for any of these models into HARP.

- Pitch shifting: [hugggof/pitch_shifter](https://huggingface.co/spaces/hugggof/pitch_shifter)
- Harmonic/percussive source separation: [hugggof/harmonic_percussive](https://huggingface.co/spaces/hugggof/harmonic_percussive)
- Music audio generation: [descript/vampnet](https://huggingface.co/spaces/descript/vampnet)
- Convert Instrumental Music into 8-bit Chiptune: [hugggof/nesquik](https://huggingface.co/spaces/hugggof/nesquik)
- Music audio generation: [hugggof/MusicGen](https://huggingface.co/spaces/hugggof/MusicGen)
- Pitch-preserving timbre-removal: [cwitkowitz/timbre-trap](https://huggingface.co/spaces/cwitkowitz/timbre-trap)


# Making a deep learning model compatible with HARP
We provide a lightweight API called [pyHARP](https://github.com/audacitorch/pyharp) for building compatible [Gradio](https://www.gradio.app) audio-processing apps with optional interactive controls. This lets deep learning model developers create user interfaces for virtually any audio processing model with only a few lines of Python code.


# Building the HARP plug-in from source code
To build the HARP plugin from scratch, perform the following steps:

clone the HARP repo
```
git clone --recurse-submodules git@github.com:audacitorch/HARP.git
cd harp
```


## Mac OS
Mac OS builds of HARP are known to work on apple silicon only. We've had trouble getting REAPER and ARA to work together on x86. TODO: test on x86 macs.

Configure
```
mkdir build
cd build
cmake ..  -DCMAKE_BUILD_TYPE=Debug
```

Build
```
make -jNUM_PROCESSORS
```

### Building for ARM vs x86 MacOS

To specify which OSX architecture you'd like to build for, set  `CMAKE_OSX_ARCHITECTURES` to either `arm64` or `x86_64`:

(for example, for an x86_64 build)
```bash
cmake .. DCMAKE_OSX_ARCHITECTURES=x86_64
```

## Windows

HARP has been tested on Windows 10 x64. You can checkout in the `windowsBuild` branch and follow the instructions there.

# Codesigning and Distribution

## Mac OS

Codesigning and packaging for distribution is done through the script located at `packaging/package.sh`.
You'll need to set up a developer account with Apple and create a certificate for signing the plugin.
For more information on codesigning and notarization for mac, refer to the [pamplejuce](https://github.com/sudara/pamplejuce) template.

The script requires the following  variables to be passed:
```
# Retrieve values from either environment variables or command-line arguments
DEV_ID_APPLICATION # Developer ID Application certificate
ARTIFACTS_PATH # should be packaging/dmg/HARP.vst3
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

# Debugging your build of HARP
### Mac
1. download visual studio code for mac https://code.visualstudio.com/
2. install Microsoft's C/C++ extension
3. open the "Run and Debug" tab in vsc, and press "create a launch.json file" using the LLDB
4. create a configuration for attaching to a process, here's an example launch.json you could use

```
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "lldb reaper",
            "type": "cppdbg",
            "request": "launch",
            "program": "/Applications/REAPER.app/Contents/MacOS/REAPER",
            "args": [],
            "cwd": "${fileDirname}",
            "MIMode": "lldb",
        }
    ]
}
```

5. build the plugin using this flag `-DCMAKE_BUILD_TYPE=Debug`
6. run the debugger and add break poitns

<!-- ## Thanks -->
<!-- Thanks to [shakfu]() for their help getting the relocatable python working for Mac OS,  and to Ryan Devens for meaningful conversations on the subject of JUCE and ARA programming.  -->
