# plugin_sandbox
A repository for a custom ARA plugin with juce, tracktion, and libtorch working together in harmony.

parts of this codebase are based on the [pamplejuce](https://github.com/sudara/pamplejuce) JUCE + CMake template. 

## Cloning
```git clone --recurse-submodules git@github.com:audacitorch/plugin_sandbox.git```

## Building
### ARA SDK
- Clone the ARA_SDK repo. Follow the instructions [here](https://github.com/Celemony/ARA_SDK)
- Make sure you checkout to the 2.2.0 release, the master branch won't work with JUCE for now.
    - git checkout releases/2.2.0
    - (don't forget its submodules)
- Update your ARA_SDK path in [CMakeLists.txt](CMakeLists.txt)


## An Important Note
For now, this build works on MacOS M1 only, since it builds with a version of relocatable python that is only available on MacOS. This is a temporary limitation (hopefully), as we figure out how to embed a Python interpreter in our windows builds. 

To see what we're doing right now, check out relocatable-python: https://github.com/gregneagle/relocatable-python
though I had to make some changes to make it work with our build system: https://github.com/hugofloresgarcia/relocatable-python

## Building

call cmake:
```
cmake  .. -DCMAKE_BUILD_TYPE=Debug
```

## Notes
these people have a cross platform embedded python using their SCons build system:
https://github.com/touilleMan/godot-python/tree/master


### CMake
#### Windows
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
#### Mac
On Mac M1 computers here are the commands you can usse for configuration and building. This project will only run on M1 Macs currently due to building issues for ARA on x86. This is building from inside of a build folder in the project.
- Configure
```
cmake ..  -DCMAKE_BUILD_TYPE=Debug 
```

-Build
```
make -jNUM_PROCESSORS
```


## Debugging
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
            "miDebuggerArgs": "--symbol-file=build/AudioPluginExampleCMAKE64_artefacts/Debug/VST3/ARA_sandbox.vst3/Contents/MacOS/ARA_sandbox"
        }
    ]
}
```
***make sure to include the `miDebuggerArgs` argument***

5. build the plugin using this flag `-DCMAKE_BUILD_TYPE=Debug`
6. run the debugger and add break poitns
