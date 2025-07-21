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

    void setResolution(int pps) { noteGrid.setResolution(pps); }
    double getResolution() { return noteGrid.getPixelsPerSecond(); }

    void setHideKeys(bool hk) { hideKeys = hk; }
    bool isHidingKeys() { return hideKeys; }

    void setHideControls(bool hC) { hideControls = hC; }
    bool isHidingControls() { return hideControls; }

    Component* getNoteGrid() { return &noteGridContainer; }

    int getKeyboardWidth() { return ! isHidingKeys() ? keyboardWidth : 0; }
    int getPianoRollSpacing() { return ! isHidingKeys() ? pianoRollSpacing : 0; }
    int getPianoRollContainerWidth();
    int getControlsWidth();
    float getKeyHeight();

    //Range<double> getVisibleMediaRange() { return visibleMediaRange; }
    Range<double> getVisibleKeyRange() { return visibleKeyRange; }

    void resizeNoteGrid(double lengthInSecs);

    void insertNote(MidiNote n) { noteGrid.insertNote(n); }
    void resetNotes() { noteGrid.resetNotes(); }

    void updateVisibleMediaRange(Range<double> newRange);

    void verticalMouseWheelMoveEvent(double deltaY);
    void verticalMouseWheelZoomEvent(double deltaZoom);

    void autoCenterViewBox(int medianMidi, float stdDevMidi);

private:
    float zoomToKeysVisible(double zoomFactor);
    double keysVisibleToZoom(float numKeysVisible);

    void updateVisibleKeyRange(Range<double> newRange);

    void visibleKeyRangeZoom(double zoomFactor);

    void scrollBarMoved(ScrollBar* scrollBarThatHasMoved, double scrollBarRangeStart) override;

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
