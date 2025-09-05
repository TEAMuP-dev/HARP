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