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

void NoteGridComponent::updateSize()
{
    setSize(static_cast<int>(pixelsPerSecond * lengthInSeconds), getHeight());
}

void NoteGridComponent::resized()
{
    const float keyHeight = getKeyHeight();

    for (auto n : midiNotes)
    {
        const float xPos = static_cast<float>(n->getStartTime() * pixelsPerSecond);
        const float width = static_cast<float>(n->getNoteLength() * pixelsPerSecond);

        const float yPos =
            static_cast<float>(getHeight()) - ((1.0f + n->getNoteNumber()) * keyHeight);
        juce::Rectangle<float> bounds(xPos, yPos, jmax(3.0f, width), keyHeight);
        n->setBounds(bounds.toNearestInt());
        // n->setBounds(xPos, yPos, jmax(3.0f, width), keyHeight);
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
