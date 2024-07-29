// Adapted from https://github.com/Sjhunt93/Piano-Roll-Editor

#ifndef PianoRollEditorComponent_hpp
#define PianoRollEditorComponent_hpp

#include "PConstants.h"
#include "KeyboardComponent.hpp"
#include "NoteGridComponent.hpp"


/*
 This custom viewport is used so that when the main
 piano roll is moved the others can also be moved.
 */
class CustomViewport : public Viewport
{
public:
    void visibleAreaChanged (const Rectangle<int>& newVisibleArea)
    {
        Viewport::visibleAreaChanged(newVisibleArea);
        if (positionMoved) {
            positionMoved(getViewPositionX(), getViewPositionY());
        }
    }

    std::function<void(int,int)> positionMoved;
};


/*
 ----------------------------------------------
 |      |                                     |
 |      |                                     |
 | keys | main viewport / note grid component |
 |      |                                     |
 |      |                                     |
 ----------------------------------------------
 */

class PianoRollEditorComponent : public Component
{
public:

    struct ExternalModelEditor {
        std::vector<NoteModel*> models; // const event pointers but mutable elements
        std::function<void()> update; // once you have made the edits then call this
    };

    PianoRollEditorComponent();

    ~PianoRollEditorComponent();

    /*
     This needs to be called after the constructor and determines the size of the grid.
     Once setup the number of bars can be dynamically altered through @updateBars(..
     Todo: automatically resize the number of bars
     */
    void setup(const int bars, const int pixelsPerBar, const int noteHeight);
    void updateBars(const int newNumberOfBars);

    void paint(Graphics&) override;
    void resized() override;

    /*
     PRE Sequence is essentially the input and output to the grid, i.e. the data model abstracted away from the GUI
     The GUI creates a PRESequence
     The GUI understands how to render a PRESequence

     Although this approach is probably inefficent, its unlikely to cause realtime performance issues...
     */
    void loadSequence(PRESequence sequence);

    void setScroll(double x, double y);
    void setZoomFactor(float factor);

private:

    CustomViewport viewportGrid;
    CustomViewport viewportPiano;

    NoteGridComponent noteGrid;
    KeyboardComponent keyboardComp;

    JUCE_DECLARE_NON_COPYABLE(PianoRollEditorComponent)
};

#endif /* PianoRollEditorComponent_hpp */
