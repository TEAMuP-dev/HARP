# plugin_sandbox
A repository for a custom ARA plugin with juce, tracktion, and libtorch working together in harmony.

## Cloning
```git clone --recurse-submodules git@github.com:audacitorch/plugin_sandbox.git```

## Building
### ARA SDK
- Clone the ARA_SDK repo. Follow the instructions [here](https://github.com/Celemony/ARA_SDK)
- Make sure you checkout to the 2.2.0 release, the master branch won't work with JUCE for now. 
    - git checkout releases/2.2.0
    - (don't forget its submodules)
- Update your ARA_SDK path in [CMakeLists.txt](CMakeLists.txt)

### Tracktion
- tracktion_engine is a submodule. The JUCE package that exists as a submodule in tracktion_engine is not needed (I think)

### Libtorch
- Libtorch is in C:\libtorch. Change your path in CMakeLists.txt accordingly
- Libtorch builds successfully with a normal VST plugin (see Frank's repo), but gives errors when used with ARA. 

### CMake
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


