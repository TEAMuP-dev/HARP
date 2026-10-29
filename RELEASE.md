## v4.0.0
### Overview
- HARP 4.0.0 replaces the model dropdown with a browsable catalog, opens each model in a tab of
  its own, adds a preview pane to the media clipboard, and reports errors far more precisely.

### Major Features
- **Home Tab and Model Catalog**:
  - Every Space of the [TEAMuP organization](https://huggingface.co/teamup-tech) is listed on the
    Home tab, grouped by category and searchable by name, task, tag, or path.
  - Cards show the description, tags, license, and deployment (e.g., ZeroGPU, sleeping) of each
    model. Spaces that cannot currently load are hidden, with the reason given on hover.
  - The listing is cached, so the Home tab populates at once and still works offline.
  - Custom paths (Spaces, Gradio URLs, or local addresses) are kept as cards until removed.
  - A Space entered as `user/name`, by its page URL, or by its `hf.space` address is loaded and
    listed as one model, with or without a trailing slash.
- **Model Tabs**:
  - Each model opens in a tab of its own, and several can be open at once.
  - The card of a loaded model is shown at the top of its tab.
- **Media Clipboard**:
  - The clipboard is shared by every tab, so the output of a model in one tab can be the input
    of a model in another.
  - A preview pane beneath the list plays the selected track.
  - Tracks can be reordered by dragging, and renamed by double-clicking their names. The
    clipboard and its preview pane can be resized.
  - Send to DAW converts audio to the format of the file it replaces.
- **Controls and Tracks**:
  - New controls for number boxes, multiselect dropdowns, and generic file inputs.
  - Generic file outputs, for models that return something other than audio or MIDI.
  - Optional inputs are marked as such.
  - A time axis on every track, and a pause button beside play and stop.
  - Zooming behaves the same for files of any length, and reaches down to a tenth of a second.
  - Output tracks can be copied for pasting into a DAW or a file browser.
  - Drum notes are drawn distinctly in MIDI tracks.
- **Error Reporting**:
  - Errors say what went wrong and what to do about it, distinguishing, e.g., a sleeping Space
    from a broken one, and GPU quota from a runtime error (on Gradio 6.13 or newer).
  - Besides the page of the model and the log, error popups can open the API key settings or a
    prefilled GitHub issue.
  - API keys are kept out of logs, and are only sent to Hugging Face and the local machine.

### Other Improvements
- **Cancellation**: Canceling stops only the job it was meant for, even when several clients
  share a Space, and no longer blocks the interface.
- **Stability**: Fixed crashes and hangs when closing tabs or quitting during a request, when
  loading MIDI files without notes, and when receiving unrecognized labels.
- **Performance**: All tracks share one audio device, and text is measured once rather than on
  every repaint.
- **Window Sizing**: Minimum window sizes are now enforced on Linux, and each tab scrolls when the
  window is too small for its contents.
- **Status Area**: Status messages are kept in a history that can be scrolled back.
- **Tutorial**: The tutorial covers the Home tab and model tabs.
- **JUCE 8**: HARP is built on JUCE 8.0.12, up from 7.0.5, which fixes launching, resizing, drag
  and drop, and file dialogs on Wayland desktops, e.g., Ubuntu 26.04.
- **Builds**: macOS builds are universal binaries for Apple silicon and Intel, and Linux builds
  are available for arm64 as well as x64. Windows builds link the Visual C++ runtime statically,
  so `HARP.exe` needs no DLLs beside it, and building on Windows no longer depends on where
  Visual Studio is installed.
- **Documentation**: The website, now at [harp.tools](https://harp.tools/), has setup guides for
  standalone use and for REAPER, Logic Pro, Pro Tools, Ableton Live, Cubase, Audacity, FL Studio,
  and Adobe Audition, and a guide to deploying models with PyHARP.
- **Model Validation**: A harness under `model_validation/` checks every Space of the TEAMuP
  organization daily, through the same calls HARP makes, including the refusal of unreadable and
  empty inputs.

### Requirements
- Supports models deployed with [`pyharp` **v0.4.2**](https://github.com/TEAMuP-dev/pyharp/releases/tag/v0.4.2) and `gradio` **v6.13** or newer.
