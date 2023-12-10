# HARP
![herofig_revised](https://github.com/audacitorch/HARP/assets/26678616/c4f5cdbb-aaff-4196-b9d2-3b6f69130856)

HARP is an [ARA](https://www.synchroarts.com/blog/what-is-ara) plug-in that allows for **h**osted, **a**synchronous, **r**emote **p**rocessing with deep learning models by routing audio from a digital audio workstation (DAW) through [Gradio](https://www.gradio.app) endpoints. Because Gradio apps can be hosted locally or in the cloud (e.g., HuggingFace Spaces), HARP lets  DAW users access large state-of-the-art models with GPU compute from the cloud without breaking their within-DAW workflow.

# Available Models
At present, the following models are available for use within HARP:
- [hugggof/pitch_shifter](https://huggingface.co/spaces/hugggof/pitch_shifter)
- [hugggof/harmonic_percussive](https://huggingface.co/spaces/hugggof/harmonic_percussive)
- [hugggof/MusicGen](https://huggingface.co/spaces/hugggof/MusicGen)
- [descript/vampnet](https://huggingface.co/spaces/descript/vampnet)
- [cwitkowitz/timbre-trap](https://huggingface.co/spaces/cwitkowitz/timbre-trap)

# Making a deep learning model compatible with HARP
We provide a lightweight API called [pyHARP](https://github.com/audacitorch/pyharp) for building compatible [Gradio](https://www.gradio.app) audio-processing apps with optional interactive controls. This lets deep learning model developers create user interfaces for virtually any audio processing model with only a few lines of Python code.

# OS and DAW compatibility
HARP has been tested on arm-based Mac computers running Mac OS 13.0, using the [REAPER](https://www.reaper.fm) digital audio workstation.

# Installing HARP
## MacOS
* Download the HARP DMG file from from the HARP [releases](https://github.com/audacitorch/HARP/releases)

* Double click on the DMG file. This will open the window below
<img width="397" alt="harp_dmg" src="https://github.com/audacitorch/HARP/assets/26678616/61acf9f3-8e00-4b85-9433-77366b262e19">

* Double click on "Your Mac's VST3 folder"

* Drag HARP.vst3 to the folder that was opened in the previous step

## Windows & Linux
The windows build is still under development. There are no current plans to support Linux

# Building the HARP plug-in from source code
To build the HARP plugin from scratch, perform the following steps:

clone the HARP repo
```
git clone --recurse-submodules git@github.com:audacitorch/HARP.git
cd harp
```


## Mac OS

Mac OS builds are known to work on apple silicon only. We've had trouble getting REAPER and ARA to work together on x86 (on apple silicon machines, to be fair). TODO: test on x86 macs.

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

### An Important Note
For now, this build works on MacOS only, since it has a custom build process that makes use of [pyinstaller](https://pyinstaller.org/en/stable/usage.html).
**TODO**: add cmake options to build on windows.

Here are the commands used in VSCode (Cmake Tools extension) and Windows 10.
Note that if you're using Reaper x64, you need to build the 64bit version of the plugin.

- Configure

```php
"C:\Program Files\CMake\bin\cmake.EXE" --no-warn-unused-cli -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -SC:/Users/xribene/Projects/audacitorch/plugin_sandbox -Bc:/Users/xribene/Projects/audacitorch/HARP/build -G "Visual Studio 17 2022" -A x64
```

```php
cmake --no-warn-unused-cli -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -S . -B build -G "Visual Studio 17 2022" -A x64
```
```
- Build
```php
"C:\Program Files\CMake\bin\cmake.EXE" --build c:/Users/xribene/Projects/audacitorch/HARP/build --config Debug --target ALL_BUILD -j 14 --

cmake --build build --config Debug --target ALL_BUILD -j 14

```

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

# Debugging
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
