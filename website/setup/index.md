# Setting Up HARP

HARP runs as a [standalone](/setup/standalone) editor, or beside a DAW. A DAW works with HARP
in one of two ways:

- **As an external sample editor.** The DAW opens the audio file behind a clip in HARP, and takes
  the result back once HARP overwrites that file. [REAPER](/setup/reaper),
  [Logic Pro](/setup/logic), [Ableton Live](/setup/ableton), and
  [FL Studio](/setup/flstudio) work this way.
- **By exchanging files.** Audio moves between the two by export and import, or by drag and drop,
  as it does with [Pro Tools](/setup/protools), [Cubase](/setup/cubase),
  [Audacity](/setup/audacity), and [Adobe Audition](/setup/audition).

## The Round Trip

With HARP set as the external editor of a DAW, processing a clip takes the same steps in each. The
DAWs name a clip differently, an item in REAPER, a region in Logic Pro, and a sample in FL Studio,
and the page for each uses its own term.

1. **Open the clip in HARP** from the DAW. The DAW passes HARP the audio file the clip plays,
   which appears in the media clipboard as a _DAW-linked_ track, marked by a purple header. If
   HARP is already open, it asks whether to open the file in a new window or the current one.
2. **Process it.** Open a model, drag the DAW-linked track onto its input track, and select
   `Process`.
3. **Send the result back.** Drag the output track into the media clipboard, select it, and select
   the send icon. Choose the DAW-linked track to replace, then select `Overwrite`. HARP writes the
   result over the file of the clip, converting it to the format of that file.
4. **Return to the DAW**, which picks up the changed file.

![Sending a result back to the DAW from the media clipboard](/images/send-to-daw.png)

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

The page for each DAW names the command that makes a copy of a clip with a file of its own.

## MIDI

Most DAWs open only audio in an external editor. To process MIDI, export the clip as a MIDI file,
or drag it out of the DAW where the DAW allows, and add it to HARP as you would audio.
Drag the result back into the DAW, or save and import it.
