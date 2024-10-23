// Adapted from https://github.com/Sjhunt93/Piano-Roll-Editor

#include "KeyboardComponent.hpp"
#include "NoteGridComponent.hpp"


/*
 --------------------
 |      |           |
 |      |           |
 | keys | note grid |
 |      |           |
 |      |           |
 --------------------
 */

class PianoRollComponent : public Component,
                           public ChangeBroadcaster,
                           private ScrollBar::Listener
{
public:

    PianoRollComponent(int _keyboardWidth=70, int _pianoRollSpacing=5, int _scrollBarSize=10, int _scrollBarSpacing=2);

    ~PianoRollComponent();

    void paint(Graphics& g) override;
    void resized() override;

    NoteGridComponent* getNoteGrid() { return &noteGrid; }

    void setResolution(int pixelsPerSecond);

    void resizeNoteGrid(double lengthInSecs);

    void updateVisibleKeyRange(Range<double> newRange);
    void updateVisibleMediaRange(Range<double> newRange);

    void scrollBarMoved(ScrollBar* scrollBarThatHasMoved, double scrollBarRangeStart) override;

    void visibleKeyRangeZoom(double amount);

    void insertNote(MidiNoteComponent n);
    void resetNotes();

    int getKeyboardWidth() { return keyboardWidth; }
    int getPianoRollWidth();
    int getPianoRollSpacing() { return pianoRollSpacing; }
    int getScrollBarSize() { return scrollBarSize; }
    int getScrollBarSpacing() { return scrollBarSpacing; }
    int getResolution() { return noteGrid.getPixelsPerSecond(); }

private:

    double zoomToKeysVisible(double zoomFactor);

    int keyboardWidth;
    int pianoRollWidth;
    int pianoRollSpacing;
    int scrollBarSize;
    int scrollBarSpacing;

    KeyboardComponent keyboard;
    NoteGridComponent noteGrid;
    Component noteGridContainer;

    Range<double> fullKeyRange = {0.0, 128.0};

    int minKeysVisible = 5;
    int maxKeysVisible = 12;
    Range<double> visibleKeyRange;

    Range<double> visibleMediaRange;

    ScrollBar verticalScrollBar{ true };

    Slider verticalZoomSlider{ Slider::LinearVertical, Slider::NoTextBox };
};
