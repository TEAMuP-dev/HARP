# Basic HARP Workflow

- Open HARP, either standalone or from a DAW.
- On the `Home` tab, browse or search the models hosted by
  [TEAMuP on Hugging Face](https://huggingface.co/teamup-tech), which are grouped by function,
  or choose `Custom Path...` and supply the URL of any HARP-compatible Gradio endpoint. A custom
  path keeps a card on the `Home` tab until it is removed with the X button on that card.
- Select a model to open it in a tab of its own. Several models can be open at once.
- Load audio or MIDI with the folder icon of an input track, or by dragging a file onto the track.
- Set the controls, then select `Process`. Results are rendered in place as they arrive.
- Save the result with the `Save` icon, or return it to the DAW with `Send-to-DAW`.

### Tracks

- Click or drag within a track to move its playback position, during playback as well. The play
  icon becomes a pause icon while the track plays, and a paused track resumes where it stopped. The
  stop icon ends playback and returns to the start.
- Scroll over a track to zoom in time, or hold `Shift` while scrolling to pan. On a MIDI track,
  hold `Ctrl` (`Cmd` on macOS) while scrolling to zoom in pitch, or `Ctrl+Shift` to move through
  pitch.
- Drag a track onto another track, into the media clipboard, or out of HARP into a DAW.
- Save an output track with its `Save` icon, or copy it with its `Copy` icon and paste it into a DAW
  or a file browser.

### Media Clipboard

The media clipboard holds tracks independently of any one model, so a result from one model can be
carried into another as its input. Show or hide it with `View > Media Clipboard`. It also opens on
its own when a file is added with `File > Open`.

- Add a file with the folder icon, or drag one in from a track or from elsewhere.
- Select a track to play it in the preview pane beneath the list, which has its own play, pause, and
  stop icons. Drag the top edge of the pane to resize it, or minimize or close it from its title
  bar. `View > Preview Pane` shows it again, as does double-clicking a track's waveform or piano roll.
- Double-click a track's name to rename it.
- Drag a track up or down to reorder the list, or onto a model's track to use it as input.
- Drag the left edge of the clipboard to change its width.
- When a DAW opened HARP with a file, the send icon overwrites that file with the selected track,
  which brings the result back into the DAW project.

In the example below, we use the [Demucs](https://github.com/facebookresearch/demucs) source-separation model to extract the vocals from a music recording.

<!-- TODO - Update video -->
<div style="max-width:720px;margin:1.5rem auto;">
  <video controls preload="metadata" width="100%">
    <source src="/content/images/logic_demucs_harp_demo.mp4" type="video/mp4" autoplay muted loop playsinline preload="metadata">
    Your browser doesn’t support the video tag.
  </video>
</div>

Note that HARP is a __destructive__ editor. If you use it to edit regions in your DAW, saving overwrites the input regions, so you may wish to pass HARP a duplicate or "bounced" region as input.
