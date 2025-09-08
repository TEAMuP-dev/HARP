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

## Upgrade to Juce v8

If and when we decide to upgrade to JUCE v8, we need to be aware of the `alertCallback` lambda function in the `MainComponent.h` file.
```cpp
auto alertCallback = [this, msgOpts, loadingError](int result)
{
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // NOTE (hugo): there's something weird about the button indices assigned by the msgOpts here
    // DBG("ALERT-CALLBACK: buttonClicked alertCallback listener activated: chosen: " << chosen);
    // auto chosen = msgOpts.getButtonText(result);
    // they're not the same as the order of the buttons in the alert
    // this is the order that I actually observed them to be.
    // UPDATE/TODO (xribene): This should be fixed in Juce v8
    // see: https://forum.juce.com/t/wrong-callback-value-for-alertwindow-showokcancelbox/55671/2
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
```

## Buttons and MultiButtons

## Icons - FontAwesome and FontAudio

## FlexBox

## Managing Collections  of Dynamically Created Objects

When working with collections of objects created with `new`, like UI components or other heap-allocated data, avoid storing raw pointers (`T*`) directly in standard containers like `juce::Array<T*>` or `std::vector<T*>`. This requires manual `delete` calls, which is error-prone and can easily lead to memory leaks or crashes if not handled perfectly (e.g., in destructors, during removal, exception handling).

**Problem Example (from `NoteGridComponent`):**

```cpp
// DON'T DO THIS: Requires manual deletion
juce::Array<MidiNoteComponent*> midiNotes; // Stores raw pointers

void NoteGridComponent::insertNote(MidiNoteComponent n)
{
    MidiNoteComponent* note = new MidiNoteComponent(n); // Manual allocation
    midiNotes.add(note); // Array doesn't own the pointer
    addAndMakeVisible(note);
    // ...
}

void NoteGridComponent::resetNotes() // Manual deletion required
{
    for (int i = 0; i < midiNotes.size(); i++)
    {
        MidiNoteComponent* note = midiNotes.getReference(i);
        removeChildComponent(note); // Still need to remove from parent
        delete note; // *** Manual delete ***
    }
    midiNotes.clear(); // Just clears pointers, doesn't delete objects
    // ...
}

NoteGridComponent::~NoteGridComponent() {
    resetNotes(); // MUST remember to call manual cleanup
}
```

This pattern is risky. If `resetNotes` isn't called, or if a note is removed from `midiNotes` without being `delete`d, memory leaks occur.

**Preferred Solutions:**

Choose one of the following approaches to ensure automatic memory management (RAII):

**Option 1: Use `juce::OwnedArray<T>` (Idiomatic JUCE)**

`juce::OwnedArray` is specifically designed to take ownership of raw pointers allocated with `new`. It automatically `delete`s the objects it holds when they are removed or when the `OwnedArray` itself is destroyed.

*   **Declaration:**
    ```cpp
    // In NoteGridComponent.h
    juce::OwnedArray<MidiNoteComponent> midiNotes;
    ```
*   **Adding Elements:**
    ```cpp
    // filepath: /Users/xribene/Projects/HARP/src/pianoroll/NoteGridComponent.cpp
    void NoteGridComponent::insertNote(MidiNoteComponent n)
    {
        MidiNoteComponent* note = new MidiNoteComponent(n); // Still use new...
        note->setInterceptsMouseClicks(false, false);
        addAndMakeVisible(note); // Add to parent component *before* transferring ownership if needed elsewhere

        midiNotes.add(note); // ...but OwnedArray takes ownership here.
                             // It will call delete later.

        resized();
        repaint();
    }
    ```
*   **Cleanup:** Automatic! No manual `delete` needed.
    ```cpp
    // filepath: /Users/xribene/Projects/HARP/src/pianoroll/NoteGridComponent.cpp
    void NoteGridComponent::resetNotes()
    {
        // Still need to remove components from the parent hierarchy
        for (auto* note : midiNotes)
            removeChildComponent(note);

        // Clearing the OwnedArray automatically deletes the objects it owns
        midiNotes.clear();

        resized();
        repaint();
    }

    // Destructor might still call resetNotes if other cleanup is needed,
    // but the core memory management is handled by midiNotes's own destructor.
    NoteGridComponent::~NoteGridComponent() { resetNotes(); }
    ```

**Option 2: Use `juce::Array<std::unique_ptr<T>>` (Modern C++)**

Use standard C++ smart pointers (`std::unique_ptr`) within a standard container. `std::unique_ptr` guarantees deletion when it goes out of scope.

*   **Declaration:**
    ```cpp
    // In NoteGridComponent.h
    #include <memory> // Required for unique_ptr
    juce::Array<std::unique_ptr<MidiNoteComponent>> midiNotes;
    // Or: std::vector<std::unique_ptr<MidiNoteComponent>> midiNotes;
    ```
*   **Adding Elements:** Use `std::make_unique`.
    ```cpp
    // filepath: /Users/xribene/Projects/HARP/src/pianoroll/NoteGridComponent.cpp
    #include <memory> // Ensure included
    // ...
    void NoteGridComponent::insertNote(MidiNoteComponent n)
    {
        auto note = std::make_unique<MidiNoteComponent>(n); // Use make_unique, NO raw new/delete
        note->setInterceptsMouseClicks(false, false);

        // Get raw pointer for non-owning uses like addAndMakeVisible
        MidiNoteComponent* notePtr = note.get();
        addAndMakeVisible(notePtr);

        // Move ownership into the array
        midiNotes.add(std::move(note));

        resized();
        repaint();
    }
    ```
*   **Cleanup:** Automatic! `std::unique_ptr` handles deletion.
    ```cpp
    // filepath: /Users/xribene/Projects/HARP/src/pianoroll/NoteGridComponent.cpp
    void NoteGridComponent::resetNotes()
    {
        // Still need to remove components from the parent hierarchy
        for (const auto& note : midiNotes) // Use .get() to access raw pointer
            removeChildComponent(note.get());

        // Clearing the array destroys the unique_ptrs, which delete the objects
        midiNotes.clear();

        resized();
        repaint();
    }

    // Destructor might still call resetNotes if other cleanup is needed,
    // but the core memory management is handled by unique_ptr destructors.
    NoteGridComponent::~NoteGridComponent() { resetNotes(); }
    ```

**Recommendation:**

*   For JUCE projects, `juce::OwnedArray` is often the simplest and most idiomatic choice for managing collections of heap-allocated JUCE objects.
*   `juce::Array<std::unique_ptr<T>>` (or `std::vector`) is the standard C++ approach and is excellent for ensuring RAII, especially in non-JUCE specific contexts or when integrating with other modern C++ code.

Please adopt one of these safer patterns to avoid manual memory management pitfalls.

---