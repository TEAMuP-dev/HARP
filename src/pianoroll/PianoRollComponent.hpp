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
    PianoRollComponent(int _keyboardWidth = 70,
                       int _pianoRollSpacing = 5,
                       int _scrollBarSize = 10,
                       int _scrollBarSpacing = 2,
                       bool _hideKeys = false);

    ~PianoRollComponent() override;

    void paint(Graphics& g) override;
    void resized() override;

    Component* getNoteGrid() { return &noteGridContainer; }

    void setResolution(int pixelsPerSecond);
    void setHideKeys(bool _hideKeys) { hideKeys = _hideKeys; }

    void resizeNoteGrid(double lengthInSecs);

    void updateVisibleKeyRange(Range<double> newRange);
    void updateVisibleMediaRange(Range<double> newRange);

    void scrollBarMoved(ScrollBar* scrollBarThatHasMoved, double scrollBarRangeStart) override;

    void visibleKeyRangeZoom(double amount);

    void verticalMouseWheelMoveEvent(float deltaY);

    void verticalMouseWheelZoomEvent(float deltaZoom);

    void autoCenterViewBox(int medianMidi, float stdDevMidi);

    void insertNote(MidiNoteComponent n);
    void resetNotes();

    int getKeyboardWidth() { return ! isHidingKeys() ? keyboardWidth : 0; }
    int getPianoRollWidth();
    int getControlWidth() { return static_cast<int>(2.5f * scrollBarSize) + 2 * scrollBarSpacing; }
    int getPianoRollSpacing() { return ! isHidingKeys() ? pianoRollSpacing : 0; }
    int getScrollBarSize() { return scrollBarSize; }
    int getScrollBarSpacing() { return scrollBarSpacing; }
    double getResolution() { return noteGrid.getPixelsPerSecond(); }
    int getMaxKeysVisible() { return maxKeysVisible; }
    bool isHidingKeys() { return hideKeys; }

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
    Viewport noteGridContainer;

    Range<double> fullKeyRange = { 0.0, 128.0 };

    int minKeysVisible = 5;
    int maxKeysVisible = 16;
    Range<double> visibleKeyRange;

    Range<double> visibleMediaRange;

    ScrollBar verticalScrollBar { true };

    Slider verticalZoomSlider { Slider::LinearVertical, Slider::NoTextBox };
};
