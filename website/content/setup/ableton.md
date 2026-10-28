# Ableton Live

Ableton Live opens samples in HARP as an external sample editor.

<!-- TODO: verify these steps in a current version of Live, then replace the screenshot stubs. They
     follow the "Destructive Sample Editing" section of the Live 12 manual. -->

## Link HARP

1. Open the settings of Live (`Live > Settings` on macOS, `Options > Settings` on Windows), and
   select the `File & Folder` tab.
2. Beside `Sample Editor`, click `Browse`, and select HARP, _e.g._, `/Applications/HARP.app` on
   macOS.

<!-- TODO: screenshot. The File & Folder settings of Live, with HARP chosen as the sample editor.
![The File & Folder settings of Live, with HARP chosen as the sample editor](/images/ableton-sample-editor.png)
-->

The manual of Live describes the setting under
[Destructive Sample Editing](https://www.ableton.com/en/live-manual/12/clip-view/).

## Open Audio in HARP

1. Select the clip, and choose `Consolidate` (`Ctrl+J` on Windows, `Cmd+J` on macOS). The
   consolidated clip plays a new sample of its own, which leaves the original untouched, along with
   every other clip that plays it.
2. Stop playback, which Live requires before a sample is edited elsewhere.
3. In the Clip View of the clip, click `Edit` in the Audio Utilities panel to open the sample in
   HARP.

   <!-- TODO: screenshot. The Clip View of an audio clip, with the Edit button of the Audio Utilities
        panel highlighted.
   ![The Edit button in the Audio Utilities panel of the Clip View](/images/ableton-edit-button.png)
   -->

4. Process the DAW-linked track and send the result back, as in
   [The Round Trip](/content/setup/#the-round-trip). Live keeps the warp markers of the clip only if
   the length of the sample has not changed.

To process part of a clip, split it (`Ctrl+E` on Windows, `Cmd+E` on macOS) and consolidate the
excerpt.
