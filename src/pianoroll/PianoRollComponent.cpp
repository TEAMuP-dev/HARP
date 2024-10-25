// Adapted from https://github.com/Sjhunt93/Piano-Roll-Editor

#include "PianoRollComponent.hpp"


PianoRollComponent::PianoRollComponent(int _keyboardWidth, int _pianoRollSpacing, int _scrollBarSize, int _scrollBarSpacing)
{
    keyboardWidth = _keyboardWidth;
    pianoRollSpacing = _pianoRollSpacing;
    scrollBarSize = _scrollBarSize;
    scrollBarSpacing = _scrollBarSpacing;

    addAndMakeVisible(keyboard);
    addAndMakeVisible(noteGridContainer);
    noteGridContainer.addAndMakeVisible(noteGrid);

    addAndMakeVisible(verticalScrollBar);
    verticalScrollBar.setAutoHide(false);
    verticalScrollBar.addListener(this);

    verticalScrollBar.setRangeLimits(fullKeyRange);
    updateVisibleKeyRange({0.0, (double) minKeysVisible});

    addAndMakeVisible(verticalZoomSlider);
    verticalZoomSlider.setRange(0.0, 1.0, 0.0);
    verticalZoomSlider.setSkewFactor(2);
    verticalZoomSlider.setValue(1.0);
    verticalZoomSlider.onValueChange = [this] { visibleKeyRangeZoom(verticalZoomSlider.getValue()); };

    resizeNoteGrid(0.0);
}

PianoRollComponent::~PianoRollComponent()
{
    resetNotes();
}

void PianoRollComponent::paint(Graphics& g)
{
    double keysVisible = zoomToKeysVisible(verticalZoomSlider.getValue());
    double keyHeight = getHeight() / keysVisible;

    keyboard.setSize(keyboard.getWidth(), 128.0 * keyHeight);
    noteGrid.setSize(noteGrid.getWidth(), 128.0 * keyHeight);

    double pixelsPerSecond;

    if (visibleMediaRange.getLength()) {
        pixelsPerSecond = getPianoRollWidth() / visibleMediaRange.getLength();
    } else {
        pixelsPerSecond = 0;
    }

    noteGrid.setResolution(pixelsPerSecond);

    double currYPosition = -visibleKeyRange.getStart() * keyHeight;
    double currXPosition = -visibleMediaRange.getStart() * pixelsPerSecond;

    keyboard.setTopLeftPosition(0, currYPosition);
    noteGrid.setTopLeftPosition(currXPosition, currYPosition);

    sendChangeMessage();
}

void PianoRollComponent::resized()
{
    keyboard.setBounds(0, 0, keyboardWidth, getHeight());
    noteGridContainer.setBounds(keyboardWidth + pianoRollSpacing, 0, getPianoRollWidth(), getHeight());

    Rectangle<int> controlsArea = getLocalBounds().removeFromRight(2 * scrollBarSize + 4 * scrollBarSpacing);

    verticalScrollBar.setBounds(controlsArea.removeFromLeft(scrollBarSize + 2 * scrollBarSpacing).reduced(scrollBarSpacing));
    verticalZoomSlider.setBounds(controlsArea.removeFromRight(scrollBarSize + 2 * scrollBarSpacing).reduced(scrollBarSpacing));
}

void PianoRollComponent::setResolution(int pixelsPerSecond)
{
    noteGrid.setResolution(pixelsPerSecond);
}

void PianoRollComponent::resizeNoteGrid(double lengthInSecs)
{
    noteGrid.updateLength(lengthInSecs);

    if (lengthInSecs) {
        noteGrid.setResolution(getPianoRollWidth() / lengthInSecs);
    } else {
        noteGrid.setResolution(0);
    }
}

void PianoRollComponent::updateVisibleKeyRange(Range<double> newRange)
{
    visibleKeyRange = fullKeyRange.constrainRange(newRange);
    verticalScrollBar.setCurrentRange(visibleKeyRange);
    DBG("visibleKeyRange=" << visibleKeyRange.getStart() << ", " << visibleKeyRange.getEnd());
    // verticalScrollBar.
}

void PianoRollComponent::updateVisibleMediaRange(Range<double> newRange)
{
    visibleMediaRange = newRange;
}

void PianoRollComponent::scrollBarMoved(ScrollBar* scrollBarThatHasMoved, double scrollBarRangeStart)
{
    if (scrollBarThatHasMoved == &verticalScrollBar) {
        updateVisibleKeyRange(visibleKeyRange.movedToStartAt(scrollBarRangeStart));
    }
}

void PianoRollComponent::visibleKeyRangeZoom(double amount)
{
    double visibilityCenter = visibleKeyRange.getStart() + visibleKeyRange.getLength() / 2.0;

    double keysVisible = zoomToKeysVisible(amount);
    double visibilityRadius = keysVisible / 2.0;

    Range<double> newRange = {visibilityCenter - visibilityRadius, visibilityCenter + visibilityRadius};

    updateVisibleKeyRange(newRange);
}

void PianoRollComponent::verticalMouseWheelMoveEvent(float deltaY)
{
    DBG("PianoRollComponent::verticalMouseWheelMoveEvent: deltaY=" << deltaY);
    double newStart = visibleKeyRange.getStart() + deltaY * visibleKeyRange.getLength() / 1.0;
    auto newRange = Range<double>(newStart, newStart + visibleKeyRange.getLength());
    updateVisibleKeyRange(newRange);
}

void PianoRollComponent::verticalMouseWheelZoomEvent(float deltaZoom, float scrollPosY)
{
    // get the current value of the verticalZoomSlider
    auto currentZoom = verticalZoomSlider.getValue();
    // update the value of the verticalZoomSlider by the deltaZoom. jlimit ensures the value stays between 0 and 1
    verticalZoomSlider.setValue(jlimit(0.0, 1.0, currentZoom + deltaZoom), dontSendNotification);
}

void PianoRollComponent::autoCenterViewBox(int medianMidi, float stdDevMidi)
{
    // make sure the range is less than pianoRoll.maxKeysVisible
    // halfRange should be the minimum of stdDevMidi and maxKeysVisible/2
    double halfRange = jmin(2 * stdDevMidi, (float)maxKeysVisible / 2);
    double invertedMedian = 127.0 - medianMidi;
    auto initialKeyRange = Range<double>( invertedMedian - halfRange, invertedMedian + halfRange);
    updateVisibleKeyRange(initialKeyRange);
    double zoomAmount = keysVisibleToZoom(halfRange * 2);
    verticalZoomSlider.setValue(zoomAmount, dontSendNotification);
}
void PianoRollComponent::insertNote(MidiNoteComponent n)
{
    noteGrid.insertNote(n);
}

void PianoRollComponent::resetNotes()
{
    noteGrid.resetNotes();
}

int PianoRollComponent::getPianoRollWidth()
{
    return jmax(0, getWidth() - keyboardWidth - pianoRollSpacing - (2 * scrollBarSize + 4 * scrollBarSpacing));
}

double PianoRollComponent::zoomToKeysVisible(double zoomFactor)
{
    return minKeysVisible + (1.0 - zoomFactor) * (maxKeysVisible - minKeysVisible);
}

double PianoRollComponent::keysVisibleToZoom(double numKeysVisible)
{
    return 1.0 - (numKeysVisible - minKeysVisible) / (maxKeysVisible - minKeysVisible);
}
