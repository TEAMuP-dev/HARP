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

class PianoRollComponent : public Component, private ScrollBar::Listener
{
public:
    PianoRollComponent(int kbw = 70, int prs = 3, int sbsz = 8, int sbsp = 1, bool hk = false);

    ~PianoRollComponent() override;

    void resized() override;

    void setResolution(int pps);
    void setHideKeys(bool hk) { hideKeys = hk; }

    Component* getNoteGrid() { return &noteGridContainer; }
    double getResolution() { return noteGrid.getPixelsPerSecond(); }
    bool isHidingKeys() { return hideKeys; }

    void resizeNoteGrid(double lengthInSecs);

    void insertNote(MidiNote n);
    void resetNotes();

    void scrollBarMoved(ScrollBar* scrollBarThatHasMoved, double scrollBarRangeStart) override;

    void verticalMouseWheelMoveEvent(float deltaY);
    void verticalMouseWheelZoomEvent(float deltaZoom);

    void visibleKeyRangeZoom(double zoomFactor);
    void updateVisibleKeyRange(Range<double> newRange);
    void updateVisibleMediaRange(Range<double> newRange);

    void autoCenterViewBox(int medianMidi, float stdDevMidi);

    int getKeyboardWidth() { return ! isHidingKeys() ? keyboardWidth : 0; }
    int getPianoRollSpacing() { return ! isHidingKeys() ? pianoRollSpacing : 0; }
    double getKeyHeight();
    int getPianoRollContainerWidth();
    int getControlsWidth() { return static_cast<int>(2.5f * scrollBarSize) + 2 * scrollBarSpacing; }

private:
    double zoomToKeysVisible(double zoomFactor);
    double keysVisibleToZoom(double numKeysVisible);

    int keyboardWidth;
    int pianoRollSpacing;
    int scrollBarSize;
    int scrollBarSpacing;

    bool hideKeys;

    KeyboardComponent keyboard;
    NoteGridComponent noteGrid;

    Viewport keyboardContainer;
    Viewport noteGridContainer;

    Range<double> visibleMediaRange;

    int minKeysVisible = 5;
    int maxKeysVisible = 16;
    Range<double> visibleKeyRange;
    Range<double> fullKeyRange = { 0.0, 128.0 };

    ScrollBar verticalScrollBar { true };

    Slider verticalZoomSlider { Slider::LinearVertical, Slider::NoTextBox };
};
