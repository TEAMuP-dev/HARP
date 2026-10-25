## v4.0.0
 ### Overview
 - HARP 4.0.0 replaces the model dropdown with a browsable catalog, opens each model in a tab of
   its own, adds a media clipboard with a preview pane, and reports errors far more precisely.

 ### Major Features
 - **Home Tab and Model Catalog**:
   - Every Space of the [TEAMuP organization](https://huggingface.co/teamup-tech) is listed on the
     Home tab, grouped by category and searchable by name, task, tag, or path.
   - Cards show the description, tags, license, and deployment (e.g., ZeroGPU, sleeping) of each
     model.
     Spaces that cannot currently load are hidden, with the reason given on hover.
   - The listing is cached, so the Home tab populates at once and still works offline.
   - Custom paths (Spaces, Gradio URLs, or local addresses) are kept as cards until removed.
 - **Model Tabs**:
   - Each model opens in a tab of its own, and several can be open at once.
   - The card of a loaded model is shown at the top of its tab, with a link to its page.
 - **Media Clipboard**:
   - Holds files independently of any model, so the output of one model can be the input of
     another. Tracks can be added, removed, renamed, reordered, and resized.
   - A preview pane plays the selected track.
   - Tracks can be dragged between the clipboard, model tracks, and other applications, and
     copied for pasting into a DAW or a file browser.
   - Send-to-DAW converts audio to the format of the file it replaces.
 - **Controls and Tracks**:
   - New controls for number boxes, multiselect dropdowns, and generic file inputs.
   - Generic file outputs, for models that return something other than audio or MIDI.
   - Optional inputs are marked as such.
   - A time axis on every track, pause and resume, and zoom and pan by scrolling.
   - Drum notes are drawn distinctly in MIDI tracks.
 - **Error Reporting**:
   - Errors say what went wrong and what to do about it, distinguishing, e.g., a sleeping Space
     from a broken one, and GPU quota from a runtime error (on Gradio 6.13 or newer).
   - Error popups can open the page of the model, the log, the API key settings, or a prefilled GitHub
     issue.
   - API keys are kept out of logs, and are only sent to Hugging Face and the local machine.
 - **Tutorial**:
   - A step-by-step tutorial highlights each part of the interface as it describes it.

 ### Other Improvements
 - **Cancellation**: Canceling stops only the job it was meant for, even when several clients
   share a Space, and no longer blocks the interface.
 - **Stability**: Fixed crashes and hangs when closing tabs or quitting during a request, when
   loading MIDI files without notes, and when receiving unrecognized labels.
 - **Performance**: All tracks share one audio device, and text is measured once rather than on
   every repaint.
 - **Window Sizing**: Minimum window sizes are enforced on Linux, and each tab scrolls when the
   window is too small for its contents.
 - **Builds**: macOS builds are universal binaries for Apple silicon and Intel, and Linux builds
   are available for arm64 as well as x64.

 ### Requirements
 - Supports models deployed with [`pyharp` **v0.4.2**](https://github.com/TEAMuP-dev/pyharp/releases/tag/v0.4.2) and `gradio` **v6.13** or newer.
