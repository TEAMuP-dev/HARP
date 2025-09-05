# Set Up HARP in Logic Pro X

We recommend following Apple's [official instructions](https://support.apple.com/guide/logicpro/use-an-external-sample-editor-lgcp2158eb9a/mac) on how to link external sample editors such as HARP into Logic Pro X. Simply provide the path to the HARP executable (`HARP.app`) that you placed in your `Applications` folder during installation.

<div style="max-width:720px;margin:1.5rem auto;">
  <video controls preload="metadata" width="100%">
    <source src="/content/images/logic_external_editor_video.mp4" type="video/mp4" autoplay muted loop playsinline preload="metadata">
    Your browser doesn’t support the video tag.
  </video>
</div>


### Open Audio with HARP in Logic Pro X

Now that we have linked HARP to Logic as an external sample editor, we can process regions from the timeline without leaving the DAW. To get started:

* (Optional) Right-click an audio region and select _Bounce in Place_. HARP will perform __destructive__ edits if you save your outputs, meaning it will overwrite input regions. By bouncing, we create a duplicate region and ensure the original is not affected.

* Select the (bounced) region and press _Shift+W_ to open HARP. The region should be automatically loaded into HARP and ready for processing.

For more instructions on how to use HARP, see our [Workflow](/content/usage/workflow.html) documentation.


