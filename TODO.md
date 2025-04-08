# HARP 3.0 TODO

## Opening HARP from the DAW
### Tasks:
- When launched from the DAW:
  - Create a **temporary input track**.
  - Load the DAW-linked file into that track automatically.
- When the user selects a model:
  - Move the temporary input to the **first compatible input track** in the model.
- Edge case:
  - If the temp input isn't compatible with the model (e.g. audio file + MIDI model), show a dialog:
    - Option 1: Delete the input file and continue.
    - Option 2: Cancel model selection and let the user choose another model.

---

## `Send to DAW` Function

**Goal:** Allow sending any output track’s result back to the DAW.

### Tasks:
- Keep track of the **original input file path** (from the DAW) and the track it belongs to.
- After a file from an output track was `sent to DAW`:  
  - Update the original input track’s `targetFilePath` to point to its latest temp file.
- `send-to-daw` should only be allowd under these conditions:
  - HARP was launched by the DAW.
  - Output is from a compatible track (audio ↔ audio, MIDI ↔ MIDI).
- Should this function be undoable?
---

## Drag & Drop Between Tracks

**Current:** Works as copy-paste (source stays intact).  
**Goal:** Make it behave like cut-paste.

### Tasks:
- On drag between tracks:
  - Copy the file to the destination.
  - Clear the source track.
  - Handle undo/redo for this action.

---

## UI Improvements

### Track Controls:
- Prevent file drops into output tracks.
- Remove "Open File…" button from output tracks.
- Add labels to dropdown controls.
- Add a `Send to DAW` button on **output tracks**:
  - Make it easy to be trigger it's visibility (depending on whether `send to DAW` is allowed).
- Remove the generic "Save" button.
- Only keep:
  - **"Save As"** – to save dumped output manually.
  - **"Send to DAW"** – fast export to original file location.

### Other
- Add an `export labels` button to save the labels in a JSON file.
---