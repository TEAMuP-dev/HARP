//
//  PianoRollEditorComponent.cpp
//  PianoRollEditor - App
//
//  Created by Samuel Hunt on 05/02/2020.
//

#include "PianoRollEditorComponent.hpp"


//==============================================================================
PianoRollEditorComponent::PianoRollEditorComponent() : noteGrid(styleSheet), controlPannel(noteGrid, styleSheet)
{
 
    //--- grid
    addAndMakeVisible(viewportGrid);
 
    viewportGrid.setViewedComponent(&noteGrid, false);
    viewportGrid.setScrollBarsShown(true, true);
    viewportGrid.setScrollBarThickness(10);
 
    
    //--- timeline
    viewportTimeline.setViewedComponent(&timelineComp, false);
    viewportTimeline.setScrollBarsShown(false, false);
    addAndMakeVisible(viewportTimeline);
    
    //--- keyboard
    viewportPiano.setViewedComponent(&keyboardComp, false);
    viewportPiano.setScrollBarsShown(false, false);
    addAndMakeVisible(viewportPiano);
    
    //once the piano roll component is scrolled then it updates the others manually
    viewportGrid.positionMoved = [this](int x, int y)
    {
        viewportTimeline.setViewPosition(x, y);
        viewportPiano.setViewPosition(x, y);
    };
    
    // addAndMakeVisible(controlPannel);
    // controlPannel.configureGrid = [this](int pixelsPerBar, int noteHeight)
    // {
    //     setup(10, pixelsPerBar, noteHeight);
    // };
    
    noteGrid.onEdit = [this]() //pass up the chain.
    {
        if (this->onEdit != nullptr) {
            this->onEdit();
        }
    };
    noteGrid.sendChange = [this](int note, int vel)
    {
        if (this->sendChange != nullptr) {
            this->sendChange(note, vel);
        }
    };
    showPlaybackMarker = false;
    playbackTicks = 0;
}

PianoRollEditorComponent::~PianoRollEditorComponent()
{
    
}

//==============================================================================
void PianoRollEditorComponent::paint (Graphics& g)
{
    g.fillAll(Colours::darkgrey.darker());

}
void PianoRollEditorComponent::paintOverChildren (Graphics& g)
{
    const int x = noteGrid.getPixelsPerBar() * (playbackTicks / (4.0 * PRE::defaultResolution));
    const int xAbsolute = viewportGrid.getViewPosition().getX();
    
    g.setColour(Colours::greenyellow);
    g.drawLine(x - xAbsolute, 0, x - xAbsolute, getHeight(), 5.0);
}
void PianoRollEditorComponent::resized()
{
    viewportGrid.setBounds(80, 50, getWidth()-90, controlPannel.isVisible() ? getHeight()-180 : getHeight() - 55);
    viewportTimeline.setBounds(viewportGrid.getX(), 5, viewportGrid.getWidth()-10, viewportGrid.getY() - 5);
    viewportPiano.setBounds(5, viewportGrid.getY(), 70, viewportGrid.getHeight()- 10);
    
    noteGrid.setBounds(0,0,4000, 20*127);
    noteGrid.setupGrid(900, 20, 10);
    timelineComp.setBounds(0, 0, 100, viewportTimeline.getHeight());
    timelineComp.setup(10, 900);
    keyboardComp.setBounds(0, 0, viewportPiano.getWidth(), noteGrid.getHeight());
    
    controlPannel.setBounds(5, viewportGrid.getBottom() + 5, getWidth() - 10, 140);
    
    
}

void PianoRollEditorComponent::showControlPannel (bool state)
{
    controlPannel.setVisible(state);
}
//void PianoRollEditorComponent::setStyleSheet (NoteGridStyleSheet style)
//{
//
//}
void PianoRollEditorComponent::setup (const int bars, const int pixelsPerBar, const int noteHeight)
{
    
    if (bars > 1 && bars < 1000) { // sensible limits..

        noteGrid.setupGrid(pixelsPerBar, noteHeight, bars);
        timelineComp.setup(bars, pixelsPerBar);
        keyboardComp.setSize(viewportPiano.getWidth(), noteGrid.getHeight());
    }
    else {
        // you might be able to have a 1000 bars but do you really need too!?
        jassertfalse;
    }
}

void PianoRollEditorComponent::updateBars (const int newNumberOfBars)
{
    if (newNumberOfBars > 1 && newNumberOfBars < 1000) { // sensible limits..
        const float pPb = noteGrid.getPixelsPerBar();
        const float nH = noteGrid.getNoteCompHeight();
        
        noteGrid.setupGrid(pPb, nH, newNumberOfBars);
        timelineComp.setup(newNumberOfBars, pPb);
        keyboardComp.setSize(viewportPiano.getWidth(), noteGrid.getHeight());
    }
    else {
        jassertfalse;
    }
}

void PianoRollEditorComponent::loadSequence (PRESequence sequence)
{
    noteGrid.loadSequence(sequence);
    
    
    // fix me, this automatically scrolls the grid
//    const int middleNote = ((sequence.highNote - sequence.lowNote) * 0.5) + sequence.lowNote;
//    const float scrollRatio = middleNote / 127.0;
//    setScroll(0.0, scrollRatio);
}

// void PianoRollEditorComponent::clearSequence()
// {
//     // noteGrid.loadSequence can be used to clear the grid by passing an empty PRESequence
//     PRESequence emptySequence;
//     noteGrid.loadSequence(emptySequence);
// }

PRESequence PianoRollEditorComponent::getSequence ()
{
    return noteGrid.getSequence();
}

void PianoRollEditorComponent::setScroll (double x, double y)
{
    viewportGrid.setViewPositionProportionately(x, y);
}
void PianoRollEditorComponent::setPlaybackMarkerPosition (const st_int ticks, bool isVisable)
{
    showPlaybackMarker = isVisable;
    playbackTicks = ticks;
    repaint();
    
}
void PianoRollEditorComponent::setZoomFactor (float factor)
{
    auto kk = factor * 1000 + 2;
    setup(10, (int)kk, 10);
}
void PianoRollEditorComponent::disableEditing (bool value)
{
    styleSheet.disableEditing = value;
    noteGrid.repaint();
}

NoteGridControlPanel & PianoRollEditorComponent::getControlPannel ()
{
    return controlPannel;
}

PianoRollEditorComponent::ExternalModelEditor PianoRollEditorComponent::getSelectedNoteModels ()
{
    ExternalModelEditor mEdit;
    mEdit.update = [this]()
    {
        noteGrid.resized();
        noteGrid.repaint();
    };
    mEdit.models = noteGrid.getSelectedModels();
    return mEdit;
}
