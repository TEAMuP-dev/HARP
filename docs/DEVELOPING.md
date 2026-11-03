# Development Guidelines

## Coding Conventions

### Formatting

`.clang-format` holds the formatting rules of the project, which follow JUCE. Format a file with
`clang-format` before committing it. In Visual Studio Code, install the C/C++ extension from
Microsoft, and on macOS `clang-format` itself with `brew install clang-format`, then add the
following to `.vscode/settings.json`:

```json
{
    "C_Cpp.clang_format_style": "file",
    "editor.formatOnSave": false,
    "editor.tabSize": 4,
}
```

`Ctrl+Shift+I`, or `Format Document` in the context menu of a file, then formats it, and
`"editor.formatOnSave": true` formats every file on save.

Formatting a file that has never been formatted rewrites its `git blame`. Put such formatting in a
commit of its own and add the hash of that commit to `.git-blame-ignore-revs`.

### Owning Collections

Do not store owning raw pointers (`T*`) to heap-allocated objects, such as UI components, in a
container like `juce::Array<T*>` or `std::vector<T*>`. Each object then needs a matching `delete` on
every path that removes it, including destructors, removal, and exceptions, and a path that misses
one leaks the object or deletes it twice.

**Example to avoid:**

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

If `resetNotes` is not called, or a note is removed from `midiNotes` without a matching `delete`,
the note leaks.

**Alternatives:**

Either container below owns its elements, and deletes each one when it is removed or when the
container is destroyed (RAII).

**Option 1: Use `juce::OwnedArray<T>` (Idiomatic JUCE)**

`juce::OwnedArray` takes ownership of pointers allocated with `new`, and calls `delete` on each
object it holds.

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

A `std::unique_ptr` deletes its object when it is destroyed, so a container of them deletes each
element along with it.

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
*   **Removing Elements:**
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

*   `juce::OwnedArray` is the idiomatic choice for collections of JUCE objects, such as components.
*   `juce::Array<std::unique_ptr<T>>`, or `std::vector<std::unique_ptr<T>>`, suits code that does
    not depend on JUCE, or that passes ownership to standard C++ interfaces.

## HARP Conventions

### Modularity
- Prefer clear separation of concerns: independent components, widgets, and services should live in their own units.
- Avoid unnecessary crosstalk between UI, model logic, and infrastructure layers.
- If a change touches many unrelated areas, it is usually a sign to introduce an abstraction (_e.g._ `SharedResourcePointer`, service objects).
- Avoid hard-coded conditionals that branch on specific models, modes (_e.g._, Gradio vs. Stability), or UI states. Favor extensibility.
- Support for a new service belongs in a `Client` subclass of its own, as [PROVIDERS.md](PROVIDERS.md) describes.
- Prefer designs in which new code fits the existing structure with minimal glue code.

### Efficiency
- Avoid unnecessary or repeated calls to `resized()`, `repaint()`, and layout invalidation. This can significantly slow down the application.
- Prefer event-driven updates over polling.
- Be mindful of blocking calls on the message thread. Long-running work belongs off the UI thread.
- Cache derived values where appropriate, but keep lifetimes explicit.
- Keep text measurement out of `paint()` and `resized()`. JUCE 8 shapes the text on every measurement, which is slow enough to notice in a Debug build. Use `ModelStyle::getTextWidth`, which measures each string once.

### Error Handling
- Use `OpResult` for anything with meaningful modes of failure (I/O, network, model execution, parsing, etc.).
- Errors should be propagated, not swallowed. Callers decide how to surface them.
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
  the `onClick` of each button, or pass explicit return values to `AlertWindow::addButton`
  (see [AlertWindow Button Return Values](#alertwindow-button-return-values)).

### Buttons & MultiButtons
- Buttons should do one thing and emit a clear intent (no hidden side effects).
- `MultiButton` should be used when states are mutually exclusive and obvious.
- FontAwesome and FontAudio can be used to create buttons with icons rather than text.
- Avoid encoding business logic inside button callbacks. Delegate to controllers or services.

### Sizing Components
- Prefer FlexBox layouts over hard-coded sizes and manual positioning.
- Components should size themselves based on content where possible.
- Avoid “magic numbers” for spacing. Centralize or derive them.
- Resizing should be stable and predictable across window sizes.

### External Code
- Use submodules for external dependencies that work out-of-the-box.
- Organize external code with clear boundaries (_i.e._ `external/`) and treat it as read-only.
- Do not modify third-party code unless absolutely necessary. Wrap it instead.
- Only include what is needed in build targets, and avoid leaking dependencies globally.

## Known JUCE Behaviors

### AlertWindow Button Return Values

JUCE assigns the return values for `MessageBoxOptions` buttons in an order that does not
match the order in which they were added. `LookAndFeel_V2::createAlertWindow` (which
`LookAndFeel_V4` delegates to) maps three buttons to `1`, `2`, `0` and two buttons to
`1`, `0`, so the last button added is always `0`.

This is still the behavior as of JUCE 8.0.12, contrary to the expectation in
[this forum thread](https://forum.juce.com/t/wrong-callback-value-for-alertwindow-showokcancelbox/55671/2).

No HARP code depends on that mapping any more, and new code should avoid it:

- `ModelTab::openErrorPopup` constructs its own `AlertWindow` and assigns the `onClick` of each
  button via `AlertWindow::getButton`, so the return values are never consulted.
- `Main.cpp` and `MediaClipboardWidget` construct their `AlertWindow` directly and pass
  explicit return values to `AlertWindow::addButton`.

Prefer one of those two approaches over reading the index passed to an
`AlertWindow::showAsync` callback.
