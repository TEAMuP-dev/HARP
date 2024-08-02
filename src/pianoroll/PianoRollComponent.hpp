// Adapted from https://github.com/Sjhunt93/Piano-Roll-Editor

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

class PianoRollComponent : public Component
{
public:

    PianoRollComponent();

    ~PianoRollComponent();

    void resizeNoteGrid(double lengthInSecs);
    void resizeKeyboard();

    void setKeyHeight(int pixelsPerKey);
    void setResolution(int pixelsPerSecond);
    //void setScroll(double x, double y);
    //void setZoomFactor(float factor);

    void paint(Graphics&) override;
    void resized() override;

    void insertNote(MidiNoteComponent n);
    void resetNotes();

private:

    int keyHeight;

    CustomViewport viewportGrid;
    CustomViewport viewportKeys;

    NoteGridComponent noteGrid;
    KeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE(PianoRollComponent)
};
