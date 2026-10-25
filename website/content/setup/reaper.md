# REAPER

REAPER opens items in HARP as an external editor.

## Link HARP

1. Open the preferences of REAPER (`Ctrl+P` on Windows and Linux, `Cmd+,` on macOS).
2. Select `External Editors`, at the bottom of the list, and click `Add`.
3. Leave `File type (extension)` blank to make HARP the default editor for every file type, or enter
   an extension such as `wav` to use HARP for that type alone.
4. Click `Browse...` beside `Primary Editor`, and select HARP, _e.g._, `/Applications/HARP.app` on
   macOS. Click `OK` in both windows.

<p align="center">
   <img width="1023" src="https://github.com/TEAMuP-dev/HARP/assets/33099118/b828a2fd-5378-490c-be37-11f7404eb127" alt="The External Editors preferences of REAPER, with HARP set as the primary editor">
</p>

## Open Audio in HARP

1. Right-click an item, and choose `Open items in editor > Open item copies in 'HARP.app'`. The
   menu names the file you selected as the editor, _e.g._, `HARP.exe` on Windows. REAPER opens a
   copy of the item, which leaves the original untouched, while `Open items in 'HARP.app'` opens
   the original.

   <p align="center">
      <img width="1531" src="https://github.com/TEAMuP-dev/HARP/assets/33099118/7f26857f-61de-4765-9671-fb69c4264dc4" alt="The item context menu of REAPER, open at Open items in editor">
   </p>

2. Process the DAW-linked track and send the result back, as in
   [The Round Trip](/content/setup/#the-round-trip).

To process part of an item, split it, then select the excerpt and choose `Glue items`, which gives
it a file of its own.
