# Debugging

We provide instructions for debugging your HARP build in [Visual Studio Code](https://code.visualstudio.com/):

1. Download [Visual Studio Code](https://code.visualstudio.com/).
2. Install the C/C++ extension from Microsoft.
3. Open the _Run and Debug_ tab in VS Code and click _create a launch.json file_ using _CMake Debugger_.
4. Create a configuration to attach to the process (see the following example code to be placed in `launch.json`).

```json5
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Standalone HARP",
            "type": "cppdbg",
            "request": "launch",
            "program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP.app", // macOS
            //"program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP.exe", // Windows
            //"program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP", // Linux
            "args": ["../test/test.wav", "../test/test.mid"],
            "cwd": "${fileDirname}",
            "MIMode": "lldb" // macOS
        },
        {
            "name": "Attach to HARP",
            "type": "cppdbg",
            "request": "attach",
            "program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP.app", // macOS
            //"program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP.exe", // Windows
            //"program": "${workspaceFolder}/build/HARP_artefacts/Debug/HARP", // Linux
            "processId": "${command:pickProcess}",
            "MIMode": "lldb" // macOS
        }
    ]
}
```

5. Build the plugin using the flag `-DCMAKE_BUILD_TYPE=Debug` (see the following configure / build commands for macOS).

```bash
# CMake Configure Command:
/opt/homebrew/bin/cmake -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE -DCMAKE_C_COMPILER:FILEPATH=/usr/bin/clang -DCMAKE_CXX_COMPILER:FILEPATH=/usr/bin/clang++ --no-warn-unused-cli -S /Users/<USER>/Projects/HARP -B /Users/<USER>/Projects/HARP/build -G Ninja
```

```bash
# CMake Build Command:
/opt/homebrew/bin/cmake --build /Users/<USER>/Projects/HARP/build --config Debug --target all --
```

6. Add break points and run the debugger.
