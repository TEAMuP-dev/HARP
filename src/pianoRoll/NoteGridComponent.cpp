#include "NoteGridComponent.hpp"
#include <array>

#ifndef LIB_VERSION
#include "DataLoggerRoot.h"
#endif


NoteGridComponent::NoteGridComponent()
{
    blackPitches.add(1);
    blackPitches.add(3);
    blackPitches.add(6);
    blackPitches.add(8);
    blackPitches.add(10);

    // TODO - get rid of this
    ticksPerTimeSignature = PRE::defaultResolution * 4; //4/4 assume
}

NoteGridComponent::~NoteGridComponent()
{
    for (int i = 0; i < noteComps.size(); i++) {
        removeChildComponent(noteComps[i]);
        delete noteComps[i];
    }
}

void NoteGridComponent::paint(Graphics& g)
{
    g.fillAll(Colours::darkgrey);

    float line = 0; // Cumulative height

    for (int i = 127; i >= 0; i--) {
        const int pitch = i % 12;

        g.setColour(blackPitches.contains(pitch) ?
                    Colours::darkgrey.withAlpha(0.5f) :
                    Colours::lightgrey.darker().withAlpha(0.5f));
        g.fillRect(0, (int) line, getWidth(), (int) noteCompHeight);

        line += noteCompHeight;

        g.setColour(Colours::black);
        g.drawLine(0, line, getWidth(), line);
    }
}

void NoteGridComponent::resized()
{
    for (auto component : noteComps) {
        // convert from model representation into component representation (translation and scale)

        const float xPos = (component->getModel().getStartTime() / ((float) ticksPerTimeSignature)) * pixelsPerBar;
        const float yPos = (getHeight() - (component->getModel().getNote() * noteCompHeight)) - noteCompHeight;

        float len = (component->getModel().getNoteLegnth() / ((float) ticksPerTimeSignature)) * pixelsPerBar;

        component->setBounds(xPos, yPos, len, noteCompHeight);
    }
}

void NoteGridComponent::setupGrid(float px, float compHeight, const int bars)
{
    pixelsPerBar = px;
    noteCompHeight = compHeight;
    setSize(pixelsPerBar * bars, compHeight * 128); //we have 128 slots for notes
}

void NoteGridComponent::loadSequence(PRESequence sq)
{
    for (int i = 0; i < noteComps.size(); i++) {
        removeChildComponent(noteComps[i]);
        delete noteComps[i];
    }
    noteComps.clear();

    noteComps.reserve(sq.events.size());

    for (auto event : sq.events) {
        PNoteComponent * nn = new PNoteComponent();

        addAndMakeVisible(nn);
        NoteModel nModel(event);
        nn->setValues(nModel);

        noteComps.push_back(nn);
    }
    resized();
    repaint();
}

float NoteGridComponent::getNoteCompHeight()
{
    return noteCompHeight;
}

float NoteGridComponent::getPixelsPerBar()
{
    return pixelsPerBar;
}
