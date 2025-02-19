## v2.1.0
### Overview
- This version extends the previous major release (HARP 2.0) with several features and critical bug fixes which greatly improve the usability and workflow of HARP.

### Key Features
- **Labels Improvement**:
  - Complete support for displaying labels, including those without height information specified, which now appear above the media display.
- **New Input Controls**:
  - Dropdown and checkbox (toggle) controls are now fully supported in HARP.
- **Drag file from HARP to DAW**:
  - It is now possible to drag and drop media from the HARP application into external applications (i.e., DAWs).

### Major BUG Fixes
- **Cancelling Functionality**:
  - Clicking the cancel button will now promptly exit the processing state, allowing for immediate subsequent processing.
- **14sec timelimit**:
  - Heartbeat messages from Gradio are now handled gracefully, removing the need for a short processing timeout window.
- **Improved UI space management**:
  - The resizing behavior and control layout of the HARP application is now much more intuitive and efficient.

### Requirements
- Works with models deployed with `pyharp` **v0.2.0** and `gradio` **v4.44.x**
