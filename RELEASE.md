## v3.0.1
 ### Overview
 - HARP 3.0.1 introduces a few minor improvements to workflow and usability.

 ### Minor Improvements
 - **Settings Window**:
   - A settings window can now be opened from the file menu. This is where most important persistent settings will be managed.
- **Token Interactions**:
   - The token logins for Hugging Face and Stability AI have been moved to settings. Previously added tokens are displayed and the ability to remove added tokens has been added.
- **Token Error Messages**:
   - The case where a user has not added a token is now detected for both Hugging Face and Stability AI models, and more intuitive error messages are given to the user.
- **Fixed Incorrect Links**:
   - The links for sleeping spaces have been fixed, along with better links on the about page and for the Stability AI models.

 ### Requirements
 - Supports models deployed with [`pyharp` **v0.3.0**](https://github.com/TEAMuP-dev/pyharp/releases/tag/v0.3.0) and `gradio` **v5.x.x**.
