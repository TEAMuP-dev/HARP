#include "NoteGridComponent.hpp"


NoteGridComponent::NoteGridComponent()
{
    includeNoteNames = false;

    lengthInSecs = 0;

    // Defaults to 100 pixels per second
    // TODO - resolution should be a function of zoom
    setResolution(100);
}

NoteGridComponent::~NoteGridComponent()
{
    resetNotes();
}

void NoteGridComponent::updateLength(double l)
{
    lengthInSecs = l;

    setSize(pixelsPerSecond * lengthInSecs, getHeight());
}

void NoteGridComponent::setResolution(float pps)
{
    pixelsPerSecond = pps;
}

void NoteGridComponent::resized()
{
    const float keyHeight = getKeyHeight();

    for (auto n : midiNotes) {
        const float xPos = ((float) n->getStartTime()) * pixelsPerSecond;
        const float width = ((float) n->getNoteLength()) * pixelsPerSecond;

        const float yPos = getHeight() - ((1 + n->getNoteNumber()) * keyHeight);

        n->setBounds(xPos, yPos, width, keyHeight);
    }
}

void NoteGridComponent::insertNotes(Array<MidiNoteComponent*> notes)
{
    for (auto n : notes) {
        midiNotes.add(n);
        addAndMakeVisible(n);
    }

    resized();
    repaint();
}

void NoteGridComponent::resetNotes()
{
    for (int i = midiNotes.size() - 1; i >=0; i++) {
        removeChildComponent(midiNotes.getReference(i));
        delete midiNotes.getReference(i);
    }

    midiNotes.clear();

    resized();
    repaint();
}
