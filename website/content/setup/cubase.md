# Cubase

Cubase opens audio in no external editor other than WaveLab, so audio moves between Cubase and HARP
as files.

## Send Audio to HARP

1. Select the event, and choose `Audio > Bounce Selection`. Choose `Replace`, which writes the
   audio of the event to a new file in the `Audio` folder of the project, and swaps the event for
   one that plays it. The original file is left untouched.
2. Drag the new file onto an input track of a model in HARP, and select `Process`.

## Bring the Result Back

Drag the output track from HARP into the Project window, which imports it as a new event at the
position where it is dropped.

<!-- TODO: screenshot. An output track being dragged from HARP into the Project window of Cubase.
![Dragging a result from HARP into the Project window of Cubase](/images/cubase-drag-result.png)
-->
