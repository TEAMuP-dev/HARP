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

### CMake
Here are the commands used in VSCode (Cmake Tools extension) and Windows 10
- Configure
    
```php
"C:\Program Files\CMake\bin\cmake.EXE" --no-warn-unused-cli -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -SC:/Users/xribene/Projects/audacitorch/plugin_sandbox -Bc:/Users/xribene/Projects/audacitorch/plugin_sandbox/build -G "Visual Studio 17 2022" -T host=x64 -A win32
```
- Build
```php
"C:\Program Files\CMake\bin\cmake.EXE" --build c:/Users/xribene/Projects/audacitorch/plugin_sandbox/build --config Debug --target ALL_BUILD -j 14 --
```

## Problems
- The generated VST3 plugin is not visible in Cubase or Ableton.
    - The suggestion to build it in Release mode also didn't work
- Still working on tracktion integration
- Libtorch --> TODO




