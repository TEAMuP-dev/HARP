// Adapted from https://github.com/Sjhunt93/Piano-Roll-Editor

#include "PianoRollEditorComponent.hpp"


PianoRollEditorComponent::PianoRollEditorComponent()
{
    viewportPiano.setViewedComponent(&keyboardComp, false);
    viewportPiano.setScrollBarsShown(false, false);
    addAndMakeVisible(viewportPiano);

    viewportGrid.setViewedComponent(&noteGrid, false);
    viewportGrid.setScrollBarsShown(true, true);
    viewportGrid.setScrollBarThickness(10);
    addAndMakeVisible(viewportGrid);

    viewportGrid.positionMoved = [this](int x, int y)
    {
        // Keep keys in sync with pianoroll
        viewportPiano.setViewPosition(x, y);
    };
}

PianoRollEditorComponent::~PianoRollEditorComponent() {}

void PianoRollEditorComponent::paint(Graphics& g)
{
    g.fillAll(Colours::darkgrey.darker());
}

void PianoRollEditorComponent::resized()
{
    viewportGrid.setBounds(80, 5, getWidth()-90, getHeight() - 5);
    viewportPiano.setBounds(5, viewportGrid.getY(), 70, viewportGrid.getHeight()- 10);

    noteGrid.setBounds(0,0,4000, 20*127);
    noteGrid.setupGrid(900, 20, 10);
    keyboardComp.setBounds(0, 0, viewportPiano.getWidth(), noteGrid.getHeight());
}

void PianoRollEditorComponent::setup(const int bars, const int pixelsPerBar, const int noteHeight)
{
    if (bars > 1 && bars < 1000) { // sensible limits..
        noteGrid.setupGrid(pixelsPerBar, noteHeight, bars);
        keyboardComp.setSize(viewportPiano.getWidth(), noteGrid.getHeight());
    } else {
        // you might be able to have a 1000 bars but do you really need too!?
        jassertfalse;
    }
}

void PianoRollEditorComponent::updateBars(const int newNumberOfBars)
{
    if (newNumberOfBars > 1 && newNumberOfBars < 1000) { // sensible limits..
        const float pPb = noteGrid.getPixelsPerBar();
        const float nH = noteGrid.getNoteCompHeight();

        noteGrid.setupGrid(pPb, nH, newNumberOfBars);
        keyboardComp.setSize(viewportPiano.getWidth(), noteGrid.getHeight());
    } else {
        jassertfalse;
    }
}

void PianoRollEditorComponent::loadSequence(PRESequence sequence)
{
    noteGrid.loadSequence(sequence);

    // fix me, this automatically scrolls the grid
//    const int middleNote = ((sequence.highNote - sequence.lowNote) * 0.5) + sequence.lowNote;
//    const float scrollRatio = middleNote / 127.0;
//    setScroll(0.0, scrollRatio);
}

void PianoRollEditorComponent::setScroll(double x, double y)
{
    viewportGrid.setViewPositionProportionately(x, y);
}

void PianoRollEditorComponent::setZoomFactor(float factor)
{
    auto kk = factor * 1000 + 2;
    setup(10, (int)kk, 10);
}
