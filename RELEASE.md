## v2.0-alpha.1
### New Features
- Customizable color for labels
- Support for `link` field in labels

## v2.0-alpha

### Overview
The v2.0-alpha release introduces significant improvements, including Gradio HTTP support, support for audio/MIDI Labeling models, MIDI playback, alongside various bug fixes and UI enhancements.

### Key Features
- **HTTP Support**: 
  - Introduced a new `GradioClient` class to manage all HTTP requests.
  - Centralized URL parser that converts various URL formats to a `SpaceInfo` object.

- **Labels Support**:
  - Now we have support for models that besides their audio/MIDI outputs, also provide label annotations that can be displayed in the HARP UI.

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

## v1.3.0

### Features
- Added MIDI support
- `Undo` and `Redo` functionality

### Fixed
- gradio-client version
- `Save as` functionality and `Save` button
- Audio waveform display 

## v1.2.3
### Added
- hugggof/vampnet-music to model list 

## v1.2.2
### Added
- New vampnet models to model list. 

## v1.2.1
### Fixed
- gradioJuceClient builds as arm64 in macos 

## v1.2.0

### Added
- Better error logging/feedback for users
- Status & Instructions areas on MouseHover events
- MenuBar actions
- Tons of bug fixes
