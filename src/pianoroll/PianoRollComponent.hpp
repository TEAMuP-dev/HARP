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

class PianoRollComponent : public Component, public ChangeBroadcaster, private ScrollBar::Listener
{
public:
    PianoRollComponent(int kbw = 70,
                       int prs = 3,
                       int sbsz = 8,
                       int sbsp = 1,
                       bool hk = false,
                       bool hC = false);

    ~PianoRollComponent() override;

    void resized() override;

    void setResolution(int pps);
    void setHideKeys(bool hk) { hideKeys = hk; }
    void setHideControls(bool hC) { hideControls = hC; }

    Component* getNoteGrid() { return &noteGridContainer; }
    double getResolution() { return noteGrid.getPixelsPerSecond(); }
    bool isHidingKeys() { return hideKeys; }
    bool isHidingControls() { return hideControls; }

    void resizeNoteGrid(double lengthInSecs);

    void insertNote(MidiNote n);
    void resetNotes();

    void scrollBarMoved(ScrollBar* scrollBarThatHasMoved, double scrollBarRangeStart) override;

    void verticalMouseWheelMoveEvent(double deltaY);
    void verticalMouseWheelZoomEvent(double deltaZoom);

    void visibleKeyRangeZoom(double zoomFactor);
    void updateVisibleKeyRange(Range<double> newRange);
    void updateVisibleMediaRange(Range<double> newRange);

    //Range<double> getVisibleMediaRange() { return visibleMediaRange; }
    Range<double> getVisibleKeyRange() { return visibleKeyRange; }

    void autoCenterViewBox(int medianMidi, float stdDevMidi);

    int getKeyboardWidth() { return ! isHidingKeys() ? keyboardWidth : 0; }
    int getPianoRollSpacing() { return ! isHidingKeys() ? pianoRollSpacing : 0; }
    double getKeyHeight();
    int getPianoRollContainerWidth();
    int getControlsWidth();

private:
    double zoomToKeysVisible(double zoomFactor);
    double keysVisibleToZoom(double numKeysVisible);

    int keyboardWidth;
    int pianoRollSpacing;
    int scrollBarSize;
    int scrollBarSpacing;

    bool hideKeys;
    bool hideControls;

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
