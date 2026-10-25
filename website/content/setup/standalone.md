# Standalone

On its own, HARP is an editor for audio and MIDI files.

1. **Open HARP**: `HARP.app` in `Applications` on macOS, `HARP.exe` on Windows, or `HARP` on Linux.
2. **Add files** with `File > Open`, which places them in the media clipboard, or drag them onto
   the window. HARP also takes files as command-line arguments, which is how a DAW opens it:

   ```bash
   HARP vocals.wav melody.mid
   ```

3. **Process.** Open a model from the Home tab, drag a file onto its input track, and select
   `Process`.
4. **Save the result** with the `Save` icon of the output track, or select a track in the media
   clipboard and choose `File > Save As`. Saving over an input replaces it, so choose a new name to
   keep the original.

<!-- TODO: screenshot. HARP used standalone, with a file from the media clipboard dragged onto the
     input track of a model and the result in its output track.
![HARP used standalone, with a file processed by a model](/images/standalone.png)
-->

The [Walkthrough](/content/usage/workflow) covers the interface in detail.
