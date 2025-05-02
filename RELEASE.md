## v2.2.0
 ### Overview
 - This release further extends HARP 2.x with additional features and a major bug fix for Windows.
 
 ### Key Features
 - **Support for Gradio v5**:
   - Spaces deployed with Gradio v5, which has minor differences w.r.t. API calls, are now supported in HARP. Unfortunately, this means spaces deployed with previous versions of Gradio are no longer compatible.
 - **Hugging Face Authentication**:
   - A file menu has been added which opens a window to enter a user's Hugging Face token, which will be included in subsequent API calls. This allows users to take advantage of their daily qouta for usage of ZeroGPU spaces.
 - **Multiple Instances**:
   - HARP can now be invoked from the DAW (or as a standalone) with more than one file, managed through a single instance. Similarly, users can open Multiple windows for separate projects. Currently, this feature only works in Windows and MacOS (Linux coming soon).
 - **Logging & Settings**:
   - Some initial persistent settings have been added, and OS-specific conventions have been adopted for logging and settings.
 
 ### BUG Fixes
 ### Major BUG Fixes
 - **Labels on Windows**:
   - Processing with a model that produces labels no longer crashes HARP on Windows.
 
 ### Requirements
 - Works with models deployed with `pyharp` **v0.2.1** and `gradio` **v5.x.x**.
