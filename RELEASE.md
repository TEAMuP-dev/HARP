## 2.0.0.alpha

# Release 2.0.0.alpha

## Overview
The 2.0.0.alpha release introduces significant improvements, including Gradio HTTP support, support for audio/MIDI Labeling models, MIDI playback, alongside various bug fixes and UI enhancements.

## Key Features
- **HTTP Support**: 
  - Introduced a new `GradioClient` class to manage all HTTP requests.
  - Centralized URL parser that converts various URL formats to a `SpaceInfo` object.

- **Labels Support**:
  - Improved processing of output labels from `pyharp`.

- **MIDI Playback**:
  - A sine synth to quickly listen to your MIDI files.
  
- **Improved Error Handling**:
  - Refactored error management with new `ErrorType` enums and a detailed `Error` object.
  - Added `OpResult` class for operation success tracking, simplifying error reporting.



- **UI Improvements**:
  - Made several UI fixes to enhance user experience.

- **General Enhancements**:
  - Code refactoring to remove obsolete code and simplify the codebase.
  - Introduced a new C++ formatting scheme based on JUCE guidelines.

## Requirements
- Requires Gradio version **4.43.0**.

## Bug Fixes
- Various bug fixes and performance improvements.

## 1.3.0

### Features
- Added MIDI support
- `Undo` and `Redo` functionality

### Fixed
- gradio-client version
- `Save as` functionality and `Save` button
- Audio waveform display 

## 1.2.3
### Added
- hugggof/vampnet-music to model list 

## 1.2.2
### Added
- New vampnet models to model list. 

## 1.2.1
### Fixed
- gradioJuceClient builds as arm64 in macos 

## 1.2.0

### Added
- Better error logging/feedback for users
- Status & Instructions areas on MouseHover events
- MenuBar actions
- Tons of bug fixes