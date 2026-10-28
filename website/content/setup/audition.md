# Adobe Audition

Audition is an audio editor of its own, so audio moves between Audition and HARP as files.

## Send Audio to HARP

1. In the Waveform Editor, select a range and choose `File > Save Selection As`, which writes it to a
   new file. In the Multitrack Editor, choose `File > Export > Multitrack Mixdown > Time Selection`
   instead.
2. Drag the new file onto an input track of a model in HARP, and select `Process`.

## Bring the Result Back

Drag the output track from HARP onto the Files panel of Audition, which imports it, as
`File > Import > File` does. From there, open it in the Waveform Editor, or drag it into a
multitrack session.

<!-- TODO: screenshot. An output track being dragged from HARP onto the Files panel of Audition.
![Dragging a result from HARP onto the Files panel of Audition](/images/audition-drag-result.png)
-->
