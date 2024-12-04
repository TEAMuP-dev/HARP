// Adapted from https://github.com/Sjhunt93/Piano-Roll-Editor

#include "NoteGridComponent.hpp"

NoteGridComponent::NoteGridComponent()
{
    pixelsPerSecond = 0.0;
    lengthInSeconds = 0.0;
}

NoteGridComponent::~NoteGridComponent() { resetNotes(); }

void NoteGridComponent::setResolution(double pps)
{
    pixelsPerSecond = pps;

    updateSize();
}

void NoteGridComponent::updateLength(double l)
{
    lengthInSeconds = l;

    updateSize();
}

void NoteGridComponent::updateSize() { setSize(pixelsPerSecond * lengthInSeconds, getHeight()); }

void NoteGridComponent::resized()
{
    const float keyHeight = getKeyHeight();

    for (auto n : midiNotes)
    {
        const float xPos = ((float) n->getStartTime()) * pixelsPerSecond;
        const float width = ((float) n->getNoteLength()) * pixelsPerSecond;

        const float yPos = getHeight() - ((1 + n->getNoteNumber()) * keyHeight);

        n->setBounds(xPos, yPos, width, keyHeight);
    }
}

void NoteGridComponent::insertNote(MidiNoteComponent n)
{
    MidiNoteComponent* note = new MidiNoteComponent(n);
    note->setInterceptsMouseClicks(false, false);
    midiNotes.add(note);

    addAndMakeVisible(note);

    resized();
    repaint();
}

void NoteGridComponent::resetNotes()
{
    for (int i = 0; i < midiNotes.size(); i++)
    {
        MidiNoteComponent* note = midiNotes.getReference(i);
        removeChildComponent(note);

        delete note;
    }

    midiNotes.clear();

    resized();
    repaint();
}
