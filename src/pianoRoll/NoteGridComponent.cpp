//
//  NoteGridComponent.cpp
//  PianoRollEditor - App
//
//  Created by Samuel Hunt on 16/08/2019.
//

#include "NoteGridComponent.hpp"
#include <array>

#ifndef LIB_VERSION
#include "DataLoggerRoot.h"
#endif


NoteGridComponent::NoteGridComponent ()
{
    blackPitches.add(1);
    blackPitches.add(3);
    blackPitches.add(6);
    blackPitches.add(8);
    blackPitches.add(10);
    addChildComponent(&selectorBox);
    setWantsKeyboardFocus(true);
    currentQValue = PRE::quantisedDivisionValues[PRE::eQuantisationValue1_32];
    lastNoteLength = PRE::quantisedDivisionValues[PRE::eQuantisationValue1_4];
    firstDrag = false;
    firstCall = false;
    lastTrigger = -1;
    ticksPerTimeSignature = PRE::defaultResolution * 4; //4/4 assume
}

NoteGridComponent::~NoteGridComponent ()
{
    for (int i = 0; i < noteComps.size(); i++) {
        removeChildComponent(noteComps[i]);
        delete noteComps[i];
    }
}

void NoteGridComponent::paint (Graphics & g)
{
    g.fillAll(Colours::darkgrey);

    const int totalBars = (getWidth() / pixelsPerBar) + 1;

    //draw piano roll background first.
    {
        float line = 0;//noteCompHeight;

        for (int i = 127; i >= 0; i--) {
            const int pitch = i % 12;
            g.setColour(blackPitches.contains(pitch) ?
                        Colours::darkgrey.withAlpha(0.5f) :
                        Colours::lightgrey.darker().withAlpha(0.5f));

            g.fillRect(0, (int)line, getWidth(), (int)noteCompHeight);
//            g.setColour(Colours::white);
//            g.drawText(String(i), 5, line, 40, noteCompHeight, Justification::left);

            line += noteCompHeight;
            g.setColour(Colours::black);
            g.drawLine(0, line, getWidth(), line);
        }
    }

    //again this is assuming 4/4
    const float increment = pixelsPerBar / 16;
    float line = 0;
    g.setColour(Colours::lightgrey);
    for (int i = 0; line < getWidth() ; i++) {
        float lineThickness = 1.0;
        if (i % 16 == 0) { //bar marker
            lineThickness = 3.0;
        } else if (i % 4 == 0) { //1/4 div
            lineThickness = 2.0;
        }
        g.drawLine(line, 0, line, getHeight(), lineThickness);

        line += increment;
    }
}

void NoteGridComponent::resized ()
{
    for (auto component : noteComps) {
        // convert from model representation into component representation (translation and scale)

        const float xPos = (component->getModel().getStartTime() / ((float) ticksPerTimeSignature)) * pixelsPerBar;
        const float yPos = (getHeight() - (component->getModel().getNote() * noteCompHeight)) - noteCompHeight;

        float len = (component->getModel().getNoteLegnth() / ((float) ticksPerTimeSignature)) * pixelsPerBar;

        component->setBounds(xPos, yPos, len, noteCompHeight);
    }
}

void NoteGridComponent::setupGrid (float px, float compHeight, const int bars)
{
    pixelsPerBar = px;
    noteCompHeight = compHeight;
    setSize(pixelsPerBar * bars, compHeight * 128); //we have 128 slots for notes
}

void NoteGridComponent::setQuantisation (const int val)
{
    if (val >= 0 && val <= PRE::eQuantisationValueTotal) {
        currentQValue = PRE::quantisedDivisionValues[val];
    } else {
        jassertfalse;
    }
}

PRESequence NoteGridComponent::getSequence ()
{
    int leftToSort = (int) noteComps.size();

    std::vector<PNoteComponent *> componentsCopy = noteComps;
    /*
     inline lambda function to find the lowest startTime
     */
    auto findLowest = [&]() -> int {
        int lowestIndex = 0;
        for (int i = 0; i < componentsCopy.size(); i++) {
            if (componentsCopy[i]->getModel().getStartTime() < componentsCopy[lowestIndex]->getModel().getStartTime()) {
                lowestIndex = i;
            }
        }
        return lowestIndex;
    };

    PRESequence seq;
    while (leftToSort) {
        const int index = findLowest();
        auto m = componentsCopy[index]->getModel();
        m.flags.state = componentsCopy[index]->getState();
        seq.events.push_back(m);
//        seq.events[seq.events.size()-1]->flags =1  //we also want the selected flags..

        componentsCopy[index] = nullptr;
        componentsCopy.erase(componentsCopy.begin() + index);
        leftToSort--;
    }
    seq.print();
    return seq;
}

void NoteGridComponent::loadSequence (PRESequence sq)
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
        nModel.sendChange = sendChange;
//        nModel.quantiseModel(PRE::defaultResolution / 8, true, true);
        nn->setValues(nModel);

        noteComps.push_back(nn);
    }
    resized();
    repaint();
}

float NoteGridComponent::getNoteCompHeight ()
{
    return noteCompHeight;
}

float NoteGridComponent::getPixelsPerBar ()
{
    return pixelsPerBar;
}
