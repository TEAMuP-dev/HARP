# TODO for Input/Output Refactoring

## Why ?

So far we have assumed that:
- there is **always** a single input and a single output media file. 
- the input and output media files are always of the same type (e.g. MIDi or Audio)
- there might be secondary **optional** inputs which we have called `Controls` (e.g. Sliders)

We now want to expand to allow for more use cases:
1. multiple input media files
2. multiple output media files
3. different types of input and output media files
4. using only secondary inputs/controls (e.g. Slider + Text prompt) without any input media files
5. return text or output_labels without any output media files 

## Questions to answer

1. Assuming a single input and single output MIDI to Audio model, do we display both input and output media components in different rows, or do we replace the input media component with the output media component?


## Modules that need to change

### HARP
1. Undo/Redo
    - We need to do more than just replace the audio/midi files. If the answer to Q1 is to replace the input media component with the output media component, then we need to re-draw the UI and bring back the input media component when we undo the action.

2. `MediaDisplayCompoment`
    - play/stop/load buttons for each media component
    - or mute/solo buttons for each media component

3. The `mediaDisplay` in `MainComponent.h` needs to be replaced with two lists of media components: `inputMediaDisplays` and `outputMediaDisplays`

### pyHARP

1. remove the `midi_in` and `midi_out` attributes from the ModelCard class
2. add both `input_components` and `output_components` as input arguments to the `build_endpoint` function. 
3. both media components as well as control components should be mentioned explicitly in the `input_components` and `output_components` lists.
    - Up until now, we ask from the users to only specify the control components (i.e Sliders) and not the input media components which was added internally by the `build_endpoint` function.
    - Similar for output, we would always return a single media file plus the output_labels, even if the app didn't generate any output_labels. Now the user decides what to return by specifying the output components explicitly.


    772
    1285

    initially, add a wav display in the inputMediaDisplays. 
    Later when we load the model, we empty the list of displays, and add whatever the model requires. Keep a counter in the webmodel of how many input and output media components are there. If output component is different than input component (midi vs audio) then we don't merge displays. If it is audio/audio or midi/midi then we expect the pyharp to tell us if we're merging or not. If we merge then one way is to add the

    working on addNewTempFile
    if the mediaDisplayComponent is for an output, then the original/target file should be an empty file of name as the label we get from pyharp

    m_ctrls in webmodel.h maybe rename them ? because they include audio_in or midi_in
    Actually, m_ctrls is of type CtrlList which is
    ```cpp
    using CtrlList = std::vector<std::pair<juce::Uuid, std::shared_ptr<Ctrl>>>;
    ```
    I'm arguing that we don't even need to define the `Ctrl` class. The webmodel.h can store the control info in the form of 
    ```cpp
    juce::Array<juce::var> inputComponents;
    juce::Array<juce::var> outputComponents;
    ```
    And then the UI components like `ControlAreaWidget.h` can use this information to create the appropriate UI components.

    Error catching is not good now. ProcessLoadingResult wasn't suppose to crash, but now we can't be sure because the populateControls and Tracks can fail if the var received from pyharp are not valid.