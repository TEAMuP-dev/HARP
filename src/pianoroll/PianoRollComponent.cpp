#include "PianoRollComponent.hpp"

PianoRollComponent::PianoRollComponent(int kbw, int prs, int sbsz, int sbsp, bool hk, bool hC)
    : keyboardWidth(kbw),
      pianoRollSpacing(prs),
      scrollBarSize(sbsz),
      scrollBarSpacing(sbsp),
      hideKeys(hk),
      hideControls(hC)
{
    addAndMakeVisible(keyboardContainer);
    keyboardContainer.setViewedComponent(&keyboard, false);
    keyboardContainer.setScrollBarsShown(false, false);

    addAndMakeVisible(noteGridContainer);
    noteGridContainer.setViewedComponent(&noteGrid, false);
    noteGridContainer.setScrollBarsShown(false, false);
    noteGridContainer.setInterceptsMouseClicks(true, false);
    noteGrid.setInterceptsMouseClicks(false, false);

    addAndMakeVisible(verticalScrollBar);
    verticalScrollBar.setAutoHide(false);
    verticalScrollBar.addListener(this);
    verticalScrollBar.setRangeLimits(fullKeyRange);
    updateVisibleKeyRange({ 0.0, static_cast<double>(minKeysVisible) });

    addAndMakeVisible(verticalZoomSlider);
    verticalZoomSlider.setRange(0.0, 1.0, 0.0);
    verticalZoomSlider.setSkewFactor(2.0);
    verticalZoomSlider.setValue(1.0);
    verticalZoomSlider.onValueChange = [this]
    { visibleKeyRangeZoom(verticalZoomSlider.getValue()); };

    resizeNoteGrid(0.0);
}

PianoRollComponent::~PianoRollComponent() { resetNotes(); }

void PianoRollComponent::resized()
{
    // Perform component resizing
    keyboardContainer.setBounds(getLocalBounds().removeFromLeft(getKeyboardWidth()));
    noteGridContainer.setBounds(getLocalBounds()
                                    .withTrimmedLeft(getKeyboardWidth() + getPianoRollSpacing())
                                    .removeFromLeft(getPianoRollContainerWidth()));

    Rectangle<int> controlsArea = getLocalBounds().removeFromRight(getControlsWidth());

    verticalScrollBar.setBounds(controlsArea.removeFromLeft(scrollBarSize + 2 * scrollBarSpacing)
                                    .reduced(0, 2 * scrollBarSpacing)
                                    .withTrimmedLeft(2 * scrollBarSpacing));
    verticalZoomSlider.setBounds(
        controlsArea.removeFromRight(static_cast<int>(1.5 * scrollBarSize)));

    double keyHeight = getKeyHeight();

    // Set size of keyboard and note grid according to key height
    keyboard.setSize(getKeyboardWidth(), static_cast<int>(128.0 * keyHeight));
    noteGrid.setSize(getPianoRollContainerWidth(), static_cast<int>(128.0 * keyHeight));

    double pixelsPerSecond = 0.0;

    if (visibleMediaRange.getLength() > 0)
    {
        // Compute current horizontal resolution based off of expected visible range
        pixelsPerSecond = getPianoRollContainerWidth() / visibleMediaRange.getLength();
    }

    // Update horizontal pianoroll resolution
    setResolution(pixelsPerSecond);

    int currYPosition = static_cast<int>(visibleKeyRange.getStart() * getKeyHeight());
    int currXPosition = static_cast<int>(visibleMediaRange.getStart() * getResolution());

    keyboardContainer.setViewPosition(0, currYPosition);
    noteGridContainer.setViewPosition(currXPosition, currYPosition);
}

int PianoRollComponent::getPianoRollContainerWidth()
{
    return jmax(0, getWidth() - getKeyboardWidth() - getPianoRollSpacing() - getControlsWidth());
}

int PianoRollComponent::getControlsWidth()
{
    return ! isHidingControls() ? static_cast<int>(2.5f * scrollBarSize) + 2 * scrollBarSpacing : 0;
}

float PianoRollComponent::getKeyHeight()
{
    // Determine key height based off of desired amount of keys visible
    double keysVisible = zoomToKeysVisible(verticalZoomSlider.getValue());
    double keyHeight = static_cast<double>(keyboardContainer.getHeight()) / keysVisible;

    return keyHeight;
}

float PianoRollComponent::zoomToKeysVisible(double zoomFactor)
{
    return static_cast<float>(minKeysVisible)
           + static_cast<float>(1.0 - zoomFactor)
                 * static_cast<float>(maxKeysVisible - minKeysVisible);
}

double PianoRollComponent::keysVisibleToZoom(float numKeysVisible)
{
    return 1.0
           - static_cast<double>(numKeysVisible - static_cast<float>(minKeysVisible))
                 / static_cast<double>(maxKeysVisible - minKeysVisible);
}

void PianoRollComponent::resizeNoteGrid(double lengthInSecs)
{
    noteGrid.setLength(lengthInSecs);

    if (lengthInSecs > 0)
    {
        noteGrid.setResolution(getPianoRollContainerWidth() / lengthInSecs);
    }
    else
    {
        noteGrid.setResolution(0);
    }
}

void PianoRollComponent::updateVisibleMediaRange(Range<double> newRange)
{
    visibleMediaRange = newRange;

    resized();
}

void PianoRollComponent::updateVisibleKeyRange(Range<double> newRange)
{
    visibleKeyRange = fullKeyRange.constrainRange(newRange);
    verticalScrollBar.setCurrentRange(visibleKeyRange);

    resized();

    sendChangeMessage();
}

void PianoRollComponent::visibleKeyRangeZoom(double zoomFactor)
{
    double visibilityCenter = visibleKeyRange.getStart() + visibleKeyRange.getLength() / 2.0;

    double keysVisible = zoomToKeysVisible(zoomFactor);
    double visibilityRadius = keysVisible / 2.0;

    Range<double> newRange = { visibilityCenter - visibilityRadius,
                               visibilityCenter + visibilityRadius };

    updateVisibleKeyRange(newRange);
}

void PianoRollComponent::verticalMouseWheelMoveEvent(double deltaY)
{
    double newStart = visibleKeyRange.getStart() + deltaY * visibleKeyRange.getLength();

    updateVisibleKeyRange(Range<double>(newStart, newStart + visibleKeyRange.getLength()));
}

void PianoRollComponent::verticalMouseWheelZoomEvent(double deltaZoom)
{
    double currentZoomFactor = verticalZoomSlider.getValue();

    verticalZoomSlider.setValue(jlimit(0.0, 1.0, currentZoomFactor + deltaZoom));
}

void PianoRollComponent::scrollBarMoved(ScrollBar* scrollBarThatHasMoved,
                                        double scrollBarRangeStart)
{
    if (scrollBarThatHasMoved == &verticalScrollBar)
    {
        updateVisibleKeyRange(visibleKeyRange.movedToStartAt(scrollBarRangeStart));
    }
}

void PianoRollComponent::autoCenterViewBox(int medianMidi, float stdDevMidi)
{
    // Make sure vertical range is less than maximum keys visible
    float halfRange = jmin(2.0f * stdDevMidi, static_cast<float>(maxKeysVisible / 2.0f));

    double zoomAmount = keysVisibleToZoom(2.0f * halfRange);

    verticalZoomSlider.setValue(zoomAmount, dontSendNotification);

    updateVisibleKeyRange(Range<double>((127.0 - medianMidi) - static_cast<double>(halfRange),
                                        (127.0 - medianMidi) + static_cast<double>(halfRange)));
}
