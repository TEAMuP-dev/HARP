# Notes for HARP developers
## Code Formatting
In file `.clang-format` you can find the code formatting rules for the project. You can use `clang-format` to format your code before committing it. We are following the guidelines used 
by `JUCE`.
If you are using `Visual Studio Code`, you need to have the c/c++ extensions by microsoft installed. 
Also install the `Clang-Format` using brew in Macos 
```bash
brew install clang-format  
```
Finally add the following configuration to your `.vscode/settings.json`:
```json
{
    // "clang-format.executable": "/opt/homebrew/bin/clang-format",
    "C_Cpp.clang_format_style": "file",
    "editor.formatOnSave": false,
    "editor.tabSize": 4,
}
```
In vscode, you can format a file by pressing `option+shift+f` or by right clicking on the file and selecting `Format Document`.
Note: you can set "editor.formatOnSave": true, if you want to format the document on save. However if the file has never been formatted before, the 
git blame will show the entire file as changed by you, and will not show the original author of the code. 
If you want to format files that haven't been formatted before, it's better to group all the formatting changes in a single commit, and then add the hash of the commit to the `.git-blame-ignore-revs` file.