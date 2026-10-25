# Setting Up HARP

HARP runs as a [standalone](/content/setup/standalone) editor, or beside a DAW. A DAW works with HARP
in one of two ways:

- **As an external sample editor.** The DAW opens a region in HARP, and takes the result back once
  HARP overwrites the file behind it.
- **By exchanging files.** Audio moves between the two by export and import, or by drag and drop.

| Setup | Works through |
| :-- | :-- |
| [Standalone](/content/setup/standalone) | No DAW |
| [REAPER](/content/setup/reaper) | External editor |
| [Logic Pro](/content/setup/logic) | External editor |
| [Pro Tools](/content/setup/protools) | Exchanging files |
| [Ableton Live](/content/setup/ableton) | External editor |
| [Cubase](/content/setup/cubase) | Exchanging files |
| [Audacity](/content/setup/audacity) | Exchanging files |
| [FL Studio](/content/setup/flstudio) | External editor |
| [Adobe Audition](/content/setup/audition) | Exchanging files |

## The Round Trip

With HARP set as the external editor of a DAW, processing a region takes the same steps in each:

1. **Open the region in HARP** from the DAW. The DAW passes HARP the audio file the region plays,
   which appears in the media clipboard as a _DAW-linked_ track, marked by a purple header. If
   HARP is already open, it asks whether to open the file in a new window or the current one.
2. **Process it.** Open a model, drag the DAW-linked track onto its input track, and select
   `Process`.
3. **Send the result back.** Drag the output track into the media clipboard, select it, and select
   the send icon. Choose the DAW-linked track to replace, then select `Overwrite`. HARP writes the
   result over the file of the region, converting it to the format of that file.
4. **Return to the DAW**, which picks up the changed file.

<!-- TODO: screenshot. The media clipboard holding a purple DAW-linked track and a processed result,
     with the "Send to DAW" dialog open and the DAW-linked track chosen in its dropdown.
![Sending a result back to the DAW from the media clipboard](/images/send-to-daw.png)
-->

## Exchanging Files

A DAW without an external editor setting exchanges files with HARP instead:

1. **Export the audio** to a file, as the page for each DAW describes.
2. **Process it.** Drag the file onto an input track of a model, or add it to the media clipboard
   with `File > Open`, and select `Process`.
3. **Bring the result back** by dragging the output track from HARP into the DAW, or by saving it
   with its `Save` icon and importing the file.

## Protecting the Original

::: danger
<!--@include: @/../README.md#warning-->
:::

The page for each DAW names the command that makes a copy of a region with a file of its own.

## MIDI

Most DAWs open only audio regions in an external editor. To process MIDI, export the region as a
MIDI file, or drag it out of the DAW where the DAW allows, and add it to HARP as you would audio.
Drag the result back into the DAW, or save and import it.
