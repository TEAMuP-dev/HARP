// Adapted from https://github.com/Sjhunt93/Piano-Roll-Editor

#include "PianoRollComponent.hpp"


PianoRollComponent::PianoRollComponent()
{
    viewportKeys.setViewedComponent(&keyboard, false);
    viewportKeys.setScrollBarsShown(false, false);
    addAndMakeVisible(viewportKeys);

    viewportGrid.setViewedComponent(&noteGrid, false);
    viewportGrid.setScrollBarsShown(true, true);
    viewportGrid.setScrollBarThickness(10);
    addAndMakeVisible(viewportGrid);

    viewportGrid.positionMoved = [this](int x, int y)
    {
        // Keep keys in sync with pianoroll
        viewportKeys.setViewPosition(x, y);
    };

    setKeyHeight(20);

    resizeKeyboard();
    resizeNoteGrid(0.0);
}

PianoRollComponent::~PianoRollComponent() {}

void PianoRollComponent::paint(Graphics& g)
{
    g.fillAll(Colours::darkgrey.darker());
}

void PianoRollComponent::resized()
{
    int currViewPositionX = viewportGrid.getViewPositionX();
    int currViewPositionY = viewportGrid.getViewPositionY();

    viewportGrid.setBounds(80, 5, getWidth() - 85, getHeight() - 10);
    viewportKeys.setBounds(5, viewportGrid.getY(), 70, viewportGrid.getHeight() - 10);

    double totalLength = noteGrid.getLengthInSeconds();

    int pianoRollWidth = totalLength * noteGrid.getPixelsPerSecond();

    resizeNoteGrid(totalLength);

    noteGrid.setBounds(0, 0, pianoRollWidth, 127 * keyHeight);
    keyboard.setBounds(0, 0, viewportKeys.getWidth(), noteGrid.getHeight());

    viewportGrid.setViewPosition(currViewPositionX, currViewPositionY);
}

void PianoRollComponent::resizeNoteGrid(double lengthInSecs)
{
    noteGrid.updateLength(lengthInSecs);

    if (lengthInSecs) {
        noteGrid.setResolution(viewportGrid.getWidth() / lengthInSecs);
    } else {
        noteGrid.setResolution(0);
    }
}

void PianoRollComponent::resizeKeyboard()
{
    keyboard.setSize(viewportKeys.getWidth(), noteGrid.getHeight());
}

void PianoRollComponent::setKeyHeight(int pixelsPerKey)
{
    keyHeight = pixelsPerKey;
}

void PianoRollComponent::setResolution(int pixelsPerSecond)
{
    noteGrid.setResolution(pixelsPerSecond);
}

// TODO
//void PianoRollComponent::setScroll(double x, double y)
//{
//    viewportGrid.setViewPositionProportionately(x, y);
//}

// TODO
//void PianoRollComponent::setZoomFactor(float factor)
//{
//    auto kk = factor * 1000 + 2;
//    setup(10, (int) kk, 10);
//}


void PianoRollComponent::insertNote(MidiNoteComponent n)
{
    noteGrid.insertNote(n);
}

void PianoRollComponent::resetNotes()
{
    noteGrid.resetNotes();
}
