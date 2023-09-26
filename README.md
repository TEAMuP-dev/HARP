# HARP
An ARA plug-in that allows for **h**osted, **a**synchronous, **r**emote **p**rocessing with deep learning models by routing audio from the DAW through Gradio endpoints.

## TODOs
- [x] make an instructional readme for pyharp
- [ ] vampnet example that can handle audio longer than 10s
- [ ] textbox example
- [ ] make the UI just a little nicer. 
- [ ] make a hero figure for the paper
- [ ] write technical appendix for the paper
- [ ] add cmake logic for building w/ pyinstaller on windows


# Download HARP

TODO

# Making a HARP App.

You don't need to build HARP from source to use HARP apps in your DAW. 
You can download pre-built HARP apps from the [HARP releases page] (TODO). 

You can write your audio processing code in python using the [pyHARP](https://github.com/audacitorch/pyharp) library.

# Building

clone the plugin_sandbox repo
```
git clone --recurse-submodules git@github.com:audacitorch/plugin_sandbox.git
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

## Windows

### An Important Note
For now, this build works on MacOS only, since it has a custom build process that makes use of [pyinstaller](https://pyinstaller.org/en/stable/usage.html). 
**TODO**: add cmake options to build on windows. 

Here are the commands used in VSCode (Cmake Tools extension) and Windows 10.
Note that if you're using Reaper x64, you need to build the 64bit version of the plugin.

- Configure

```php
"C:\Program Files\CMake\bin\cmake.EXE" --no-warn-unused-cli -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -SC:/Users/xribene/Projects/audacitorch/plugin_sandbox -Bc:/Users/xribene/Projects/audacitorch/plugin_sandbox/build -G "Visual Studio 17 2022" -T host=x64 -A win64
```
- Build
```php
"C:\Program Files\CMake\bin\cmake.EXE" --build c:/Users/xribene/Projects/audacitorch/plugin_sandbox/build --config Debug --target ALL_BUILD -j 14 --
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
ARTIFACTS_PATH # should be packaging/dmg/tensorjuce.vst3
PROJECT_NAME # "tensorjuce"
PRODUCT_NAME # "tensorjuce"
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
