# plugin_sandbox
A repository for a custom ARA plugin with juce, tracktion, and libtorch working together in harmony.

parts of this codebase (mostly CI) are based on the [pamplejuce](https://github.com/sudara/pamplejuce) JUCE + CMake template. 

## Building

### First, An Important Note
For now, this build works on MacOS only, since it has a custom build process that makes use of [pyinstaller](https://pyinstaller.org/en/stable/usage.html). 
**TODO**: add cmake options to build on windows. 

clone the plugin_sandbox repo
```
git clone --recurse-submodules git@github.com:audacitorch/plugin_sandbox.git
```

## Windows
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

## Mac OS

On Mac M1 computers here are the commands you can usse for configuration and building. This project will only run on M1 Macs currently due to building issues for ARA on x86. 

- Configure
```
mkdir build
cd build
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
        }
    ]
}
```

5. build the plugin using this flag `-DCMAKE_BUILD_TYPE=Debug`
6. run the debugger and add break poitns

<!-- ## Thanks -->
<!-- Thanks to [shakfu]() for their help getting the relocatable python working for Mac OS,  and to Ryan Devens for meaningful conversations on the subject of JUCE and ARA programming.  -->
