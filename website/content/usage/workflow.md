# Basic HARP Workflow

HARP supports a simple workflow: pick an existing model for processing or provide a URL to your own, load the model and it's corresponding interface, tweak controls to your liking, and process.

To get started:

- Open HARP as a standalone application or within your DAW
- Select an existing model using the drop-down menu at the top of the screen, or select `custom path...` and provide a URL to any HARP-compatible Gradio endpoint 
- Load the selected model (and its corresponding interface) using the `Load` button.
- Import audio or MIDI data to process with the model either via the `Open File` button or by dragging and dropping a file into HARP
- Adjust controls to taste in the interface
- Click `Process` to run the model; outputs will automatically be rendered in HARP
- To save your outputs, click the `Save` button or select `Save As` from the `File` menu

In the example below, we use the [Demucs](https://github.com/facebookresearch/demucs) source-separation model to extract the vocals from a music recording.

<!-- TODO - Update video -->
<div style="max-width:720px;margin:1.5rem auto;">
  <video controls preload="metadata" width="100%">
    <source src="/content/images/logic_demucs_harp_demo.mp4" type="video/mp4" autoplay muted loop playsinline preload="metadata">
    Your browser doesn’t support the video tag.
  </video>
</div>

Note that HARP is a __destructive__ editor -- if you use it to edit regions in your DAW, saving will automatically overwrite input regions. You may therefore wish to create a duplicate or "bounced" region to pass to HARP as input.