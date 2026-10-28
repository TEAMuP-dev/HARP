# Logic Pro

Logic Pro opens regions in HARP as an external sample editor.

## Link HARP

1. Choose `Logic Pro > Settings > Audio`, and open the `File Editor` tab.
2. Beside `External Sample Editor`, select `HARP.app` in your `Applications` folder.

The guide of Apple on [using an external sample editor](https://support.apple.com/guide/logicpro/use-an-external-sample-editor-lgcp2158eb9a/mac)
covers the setting in more detail, and the video below walks through it.

<div style="max-width:720px;margin:1.5rem auto;">
  <video controls preload="metadata" width="100%">
    <source src="/content/images/logic_external_editor_video.mp4" type="video/mp4">
    Your browser does not support the video tag.
  </video>
</div>

## Open Audio in HARP

1. Right-click the region, and choose `Bounce in Place...` (`Ctrl+B`). The bounced region plays a
   new file of its own, which leaves the original untouched.

   <img src="/content/images/logic-bounce-in-place.png" alt="The region context menu of Logic Pro, with Bounce In Place selected" style="display:block;margin:1rem auto;max-width:640px;width:100%;">

2. Select the bounced region, and press `Shift+W` to open it in HARP.
3. Process the DAW-linked track and send the result back, as in
   [The Round Trip](/content/setup/#the-round-trip). Logic Pro updates every region that plays
   the file.

To process part of a region, split it at the playhead (`Cmd+T`) before bouncing the excerpt.
