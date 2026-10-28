# Pro Tools

Pro Tools has no external editor setting, so audio moves between Pro Tools and HARP as files.

## Send Audio to HARP

1. To send part of a clip, select the range and choose `Edit > Consolidate Clip`, which writes it
   to a file of its own.
2. Select the clips in the Clips List, and choose `Export Clips as Files...` from the Clips List
   menu. Choose WAV, the bit depth and sample rate of the session, and a destination folder.
3. Drag the exported file onto an input track of a model in HARP, and select `Process`.

## Bring the Result Back

Drag the output track from HARP into an empty area of the Edit window, which places it on a new
track, or into the Clips List, which imports it without placing it. Saving the result and choosing
`File > Import > Audio` works as well.

<!-- TODO: screenshot. An output track being dragged from HARP into the Edit window of Pro Tools.
![Dragging a result from HARP into the Edit window of Pro Tools](/images/protools-drag-result.png)
-->
