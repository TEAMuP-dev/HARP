# Developer Notes

<!-- Coding Conventions -->

## Coding Conventions

### Formatting

In file `.clang-format` you can find code formatting rules for the project.
You can use `clang-format` to format your code before committing it.
We are following the guidelines used by JUCE.
If you are using Visual Studio Code (VS Code), you must have the C/C++ extensions from Microsoft installed.
In MacOS, you should also install `clang-format` with `brew`:
```bash
brew install clang-format
```

Finally, add the following lines to your `.vscode/settings.json` file:
```json
{
    "C_Cpp.clang_format_style": "file",
    "editor.formatOnSave": false,
    "editor.tabSize": 4,
}
```

In VS Code, you can format a file by pressing `CTRL+Shift+I` or by right-clicking within the file and selecting `Format Document`.
You could also set `"editor.formatOnSave": true` in `.vscode/settings.json` if you want to format files upon saving them.

Note: if a file has never been formatted before, it may overwrite the entire `git blame` with the formatting changes.
If you want to format files that haven't been formatted before, it's better to group all the formatting changes into a single commit and then add the hash of the commit to file `.git-blame-ignore-revs` to avoid this issue.

### Managing Collections of Dynamically Created Objects

When working with collections of objects created with `new`, such as UI components or other heap-allocated data, avoid storing raw pointers (`T*`) directly in standard containers like `juce::Array<T*>` or `std::vector<T*>`.
This requires manual `delete` calls, and can easily lead to memory leaks or crashes if handled imperfectly (_e.g._, in destructors, during removal, with exceptions, etc.).

**Problem Example (from `NoteGridComponent`):**

```cpp
// DON'T DO THIS: Requires manual deletion
juce::Array<MidiNoteComponent*> midiNotes; // Stores raw pointers

void NoteGridComponent::insertNote(MidiNoteComponent n)
{
    MidiNoteComponent* note = new MidiNoteComponent(n); // Manual allocation
    midiNotes.add(note); // Array doesn't own the pointer
    addAndMakeVisible(note);
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
}

NoteGridComponent::~NoteGridComponent() {
    resetNotes(); // MUST remember to call manual cleanup
}
```

This pattern is risky. If `resetNotes` isn't called, or if a note is removed from `midiNotes` without a corresponding call to `delete`, memory leaks occur.

**Preferred Solutions:**

Choose one of the following approaches to ensure automatic memory management (RAII):

**Option 1: Use `juce::OwnedArray<T>` (Idiomatic JUCE)**

`juce::OwnedArray` is specifically designed to take ownership of raw pointers allocated with `new`. It automatically calls `delete` on the objects it holds when they are removed or when the `OwnedArray` itself is destroyed.

*   **Declaration:**
    ```cpp
    juce::OwnedArray<MidiNoteComponent> midiNotes;
    ```
*   **Adding Elements:**
    ```cpp
    void NoteGridComponent::insertNote(MidiNoteComponent n)
    {
        MidiNoteComponent* note = new MidiNoteComponent(n); // Still use `new`
        addAndMakeVisible(note); // Add to parent component *before* transferring ownership if needed elsewhere

        midiNotes.add(note); // OwnedArray takes ownership here
    }
    ```
*   **Removing Elements:**
    ```cpp
    void NoteGridComponent::resetNotes()
    {
        // Still need to remove components from parent hierarchy
        for (auto* note : midiNotes)
            removeChildComponent(note);

        // Clearing OwnedArray automatically deletes objects it owns
        midiNotes.clear();
    }

    // Destructor may still call resetNotes if other cleanup is needed,
    // but core memory management is handled by OwnedArray destructor.
    NoteGridComponent::~NoteGridComponent() { resetNotes(); }
    ```

**Option 2: Use `juce::Array<std::unique_ptr<T>>` (Modern C++)**

Use standard C++ smart pointers (`std::unique_ptr`) within a standard container. `std::unique_ptr` guarantees deletion when it goes out of scope.

*   **Declaration:**
    ```cpp
    #include <memory> // Required for unique_ptr

    juce::Array<std::unique_ptr<MidiNoteComponent>> midiNotes;
    // Or: std::vector<std::unique_ptr<MidiNoteComponent>> midiNotes;
    ```
*   **Adding Elements:**
    ```cpp
    #include <memory>

    void NoteGridComponent::insertNote(MidiNoteComponent n)
    {
        auto note = std::make_unique<MidiNoteComponent>(n); // Use `std::make_unique` (no raw `new` or `delete`)

        // Get raw pointer for non-owning uses like addAndMakeVisible
        MidiNoteComponent* notePtr = note.get();
        addAndMakeVisible(notePtr);

        // Move ownership into array
        midiNotes.add(std::move(note));
    }
    ```
*   **Cleanup:** Automatic! `std::unique_ptr` handles deletion.
    ```cpp
    void NoteGridComponent::resetNotes()
    {
        // Still need to remove components from parent hierarchy
        for (const auto& note : midiNotes)
            removeChildComponent(note.get()); // Use .get() to access raw pointer

        // Clearing array destroys all instances of unique_ptr, which deletes corresponding objects
        midiNotes.clear();
    }

    // Destructor may still call resetNotes if other cleanup is needed,
    // but core memory management is handled by unique_ptr destructor.
    NoteGridComponent::~NoteGridComponent() { resetNotes(); }
    ```

**Recommendation:**

*   For JUCE projects, `juce::OwnedArray` is often the simplest and most idiomatic choice for managing collections of heap-allocated JUCE objects.
*   `juce::Array<std::unique_ptr<T>>` (or `std::vector`) is the standard C++ approach and is excellent for ensuring RAII, especially in non-JUCE specific contexts or when integrating with other modern C++ code.

<!-- Project Conventions -->

## HARP Conventions

<!-- The following was generated and adapted from ChatGPT based on some loose notes. -->

### Modularity
- Prefer clear separation of concerns: independent components, widgets, and services should live in their own units.
- Avoid unnecessary crosstalk between UI, model logic, and infrastructure layers.
- If a change touches many unrelated areas, it’s usually a sign to introduce an abstraction (_e.g._ `SharedResourcePointer`, service objects).
- Avoid hard-coded conditionals that branch on specific models, modes (_e.g._, Gradio vs. Stability), or UI states—favor extensibility.
- Good design should feel “boring”: things fall neatly into place with minimal glue code.

### Efficiency
- Avoid unnecessary or repeated calls to `resized()`, `repaint()`, and layout invalidation. This can significantly slow down the application.
- Prefer event-driven updates over polling.
- Be mindful of blocking calls on the message thread. Long-running work belongs off the UI thread.
- Cache derived values where appropriate, but keep lifetimes explicit.

### Error Handling
- Use `OpResult` for anything with meaningful modes of failure (I/O, network, model execution, parsing, etc.).
- Errors should be propagated, not swallowed—callers decide how to surface them.
- `jassertfalse` is acceptable for truly unreachable states (logic errors, violated invariants).

### Logging & Debugging
- Use the `DBG_AND_LOG` macro to log to console and file simultaneously.
- Log at system boundaries: network calls, job lifecycle changes, model execution, and cancellation.
- Prefer structured log messages and include any contextual information (_e.g._ `"ModelTab::cancelCallback: Canceling process \"" + String(currentProcessID) + "\"."`).

### Windows & Popups
- Popups are transient and contextual (errors, selections, etc.).
- Windows are persistent, user-managed, and represent full workflows or views.
- Be consistent about ownership and lifetime: who creates, shows, and destroys.
- Launch behavior (modal vs non-modal) should be predictable across the app.
- Do not rely on the button index passed to an `AlertWindow::showAsync` callback. Bind
  each button's `onClick`, or pass explicit return values to `AlertWindow::addButton`
  (see [AlertWindow Button Return Values](#alertwindow-button-return-values)).

### Buttons & MultiButtons
- Buttons should do one thing and emit a clear intent (no hidden side effects).
- `MultiButton` should be used when states are mutually exclusive and obvious.
- FontAwesome and FontAudio can be used to create buttons with icons rather than text.
- Avoid encoding business logic inside button callbacks—delegate to controllers/services.

### Sizing Components
- Prefer FlexBox layouts over hard-coded sizes and manual positioning.
- Components should size themselves based on content where possible.
- Avoid “magic numbers” for spacing—centralize or derive them.
- Resizing should be stable and predictable across window sizes.

### External Code
- Use submodules for external dependencies that work out-of-the-box.
- Organize external code with clear boundaries (_i.e._ `external/`) and treat it as read-only.
- Do not modify third-party code unless absolutely necessary. Wrap it instead.
- Only include what is needed in build targets—avoid leaking dependencies globally.

## Known JUCE Behaviors

### AlertWindow Button Return Values

JUCE assigns the return values for `MessageBoxOptions` buttons in an order that does not
match the order in which they were added. `LookAndFeel_V2::createAlertWindow` (which
`LookAndFeel_V4` delegates to) maps three buttons to `1`, `2`, `0` and two buttons to
`1`, `0`, so the last button added is always `0`.

This is still the behavior as of JUCE 8.0.12, contrary to the expectation in
[this forum thread](https://forum.juce.com/t/wrong-callback-value-for-alertwindow-showokcancelbox/55671/2).

No HARP code depends on that mapping any more, and new code should avoid it:

- `ModelTab::openErrorPopup` constructs its own `AlertWindow` and assigns each button's
  `onClick` via `AlertWindow::getButton`, so the return values are never consulted.
- `Main.cpp` and `MediaClipboardWidget` construct their `AlertWindow` directly and pass
  explicit return values to `AlertWindow::addButton`.

Prefer one of those two approaches over reading the index passed to an
`AlertWindow::showAsync` callback.

---
