
# HARP LBD and v1.4

## Work Done in `cb/http-gradio` branch

- Requires gradio >= 4.43.0
- Deleted anything related to python.
- Refactoring and cleaning of obsolete code
- Created new `GradioClient` class that handles all the http requests to a gradio app.
- Added a new centralized space-url parser that converts all possible url formats to a `SpaceInfo` object.  
    These formats can be
    - gradio app hosted locally
        - "[http://localhost:7860](http://localhost:7860/)"
        - "http://127.0.0.1:7860"
        - https://022d82652a07ce9c53.gradio.live
    - embedded gradio app in HF
        - "[https://xribene-midi-pitch-shifter.hf.space/](https://xribene-midi-pitch-shifter.hf.space/)"
    - HF Space/repo
        - "[https://huggingface.co/spaces/xribene/midi_pitch_shifter](https://huggingface.co/spaces/xribene/midi_pitch_shifter)"
    - `username / space`
        - I.e `xribene/midi_pitch_shifter`,

- Added objects that map the OutputLabel classes in pyharp
- Parsing OutputLabel data from pyharp
- added a c++ formating scheme based on JUCE guidelines using clang-format. Instructions on how to use it are in `DevNotes.md`
- New and more fine-grained model status flags. Check `ModelStatus` in `utils.h`
- Refactored error handling.
    - Added new `ErrorType` enums and an `Error` object. The `Error` object contains an `ErrorType`, a detailed error message (`devMessage`), and a corresponding user-friendly error message (`userMessage`).
    - A  function `fillUserMessage` that converts a `devMessage` to `userMessage`. All `devMessages` go through this function before displayed to the user. 
    - A new class `OpResult` which is returned by many functions (i.e model->load(), model->process() etc). It lets us know if the operation was successful or not ( OpResult::wasOk(), OpResult::failed() ) and it also contains the `Error` object in case of failure
- Various bug fixes and improvements
	-  [Cancelling models hangs](https://github.com/TEAMuP-dev/HARP/issues/219)  
		- improved, but can't be solved for now (read comment in the issue)
	- [Juce var was void error message](https://github.com/TEAMuP-dev/HARP/issues/218 )
	-  [Gray out load button when no model path is selected.](https://github.com/TEAMuP-dev/HARP/issues/212)  
		- it still fails some times. can't figure out why
	- ["Cancel" button erases current model](https://github.com/TEAMuP-dev/HARP/issues/168)
		- This was fixed by Nathan, but I had to change the logic to match with the rest of the fixes. 
	-  [Wrong model status reported by mModelStatusTimer](https://github.com/TEAMuP-dev/HARP/issues/224) 
	-  [TextEditor in customModelPath should grab keyboard focus automatically](https://github.com/TEAMuP-dev/HARP/issues/225) 
	-  [After a customPath loading failure, the previous successfully loaded model is deleted](https://github.com/TEAMuP-dev/HARP/issues/226)


## HARP-v1.4.alpha.0

- You can find the executables [here](https://github.com/TEAMuP-dev/HARP/releases/tag/v1.4.alpha.0)
- Our current models in HF don't work with 1.4. In order to do so, these are the changes that need to be done:
	- 1. update the `requirements.txt` so that it points to the `overlays` branch of pyharp
	- 2. update the `Readme.md` so that gradio is `4.44`
	- 3. process_fn needs to return  an extra `LabelList()` output
	- 4. update the `with gr.Blocks() as demo`: block of each space, since the parameterization of `build_endpoint` has changed.
- For now, these are the upgraded spaces you can test
	- "xribene/pitch_shifter"
		- this is a PAUSED space, so you should get the corresponding error message
	- "xribene/pitch_shifter_slow"
		- this space has a 30sec wait time in its processing function. We can use that to simulate slow processing (i.e audio generation in cpu) and test the "cancel" button.
	- "xribene/pitch_shifter_awake"
		- That's our regular audio pitch_shifter that stays awake and doesn't have the 30sec wait time.
	- "xribene/midi_pitch_shifter"
		- A regular midi pitch_shifter to test MIDI I/O
### Discussion
- We should fork at least one vampnet variant in GPU and upgrade it to test it with HARP-v1.4.alpha.0
- After that, we need to do a full demo rehearsal exactly as we'll do it in ISMIR. 
- During the demo, make sure we don't have any CPU running models.
- Decide the timeout duration for http requests. Right now I've set every request to timeout after 10seconds (file upload or download, get or post requests etc). This means, that if processing of a file takes more than the timeout, the request stops, and the user gets an error message (nothing breaks or freezes though).


## "Cancel" in Gradio
Read the comments in https://github.com/TEAMuP-dev/HARP/issues/219

#### Conclusion
Try to not demo the "cancel" button in ISMIR. 
if you click "cancel", we still have to way for the remaining timeout of the "processing" request to expire.
How much you have to wait after clicking "cancel" depends on what part of the timeout passed between clicking "processing" and "cancel" 
