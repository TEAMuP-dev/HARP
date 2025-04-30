## v2.2.0
 ### Overview
 - This release further extends HARP 2.x with additional features and a major bug fix for Windows.
 
 ### Key Features
 - **Support for Gradio v5**:
   - Spaces deployed with Gradio v5, which has minor differences w.r.t. API calls, are now supported in HARP. Unfortunately, this means spaces deployed with previous versions of Gradio are no longer compatible.
 - **Hugging Face Authentication**:
   - A file menu has been added which opens a window to enter a user's Hugging Face token, which will be included in subsequent API calls.
 - **Multiple Instances**:
   - TODO
 - **Logging**:
   - TODO
 
 ### BUG Fixes
 ### Major BUG Fixes
 - **Labels on Windows**:
   - Processing with a model that produces labels no longer crashes HARP on Windows.
 
 ### Requirements
 - Works with models deployed with `pyharp` **v0.2.1** and `gradio` **v5.x.x**.
