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