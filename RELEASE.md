## v2.0.0
### Overview
The v2.0.0 release introduces significant improvements, including Gradio HTTP support, support for audio/MIDI Labeling models, MIDI playback, alongside various bug fixes and UI enhancements.

### Key Features
- **HTTP Support**: 
  - Introduced a new `GradioClient` class to manage all HTTP requests.
  - Centralized URL parser that converts various URL formats to a `SpaceInfo` object.

- **Labels Support**:
  - Now we have support for models that besides their audio/MIDI outputs, also provide label annotations that can be displayed in the HARP UI.
  - Colors and clickable external links can be added to the labels.

- **MIDI Playback**:
  - A sine synth to quickly listen to your MIDI files.

- **UI Improvements and Improved Error Handling**:
  - Refactored error handling using enums.
  - Added `OpResult` class for operation success tracking, simplifying error reporting.
  - Each error is has a `devMessage` and `userMessage` to help developers and users understand the error.
  - Several UI fixes to enhance user experience.

### Requirements
- Works with models made with Gradio version **4.43.0** and later.

### Bug Fixes
- Various bug fixes and performance improvements.