# Audacity

Audacity is an audio editor of its own, so audio moves between Audacity and HARP as files.

## Send Audio to HARP

1. Select the audio, and choose `File > Export Audio` (`Ctrl+Shift+E`), with `Export Range` set to
   `Current Selection`.
2. Drag the exported file onto an input track of a model in HARP, and select `Process`.

## Bring the Result Back

Drag the output track from HARP into Audacity's project window, which imports it as a new
track, as `File > Import > Audio` does.

<!-- TODO: screenshot. An output track being dragged from HARP into an Audacity project.
![Dragging a result from HARP into an Audacity project](/images/audacity-drag-result.png)
-->
