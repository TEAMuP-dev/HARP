#include "NoteGridComponent.hpp"

NoteGridComponent::NoteGridComponent()
{
    pixelsPerSecond = 0.0;
    lengthInSeconds = 0.0;
}

NoteGridComponent::~NoteGridComponent()
{
    //resetNotes();
}

void NoteGridComponent::paint(Graphics& g)
{
    // Paint key background
    KeyboardComponent::paint(g);

    // Paint all notes
    for (auto n : midiNotes)
    {
        const float noteWidth = static_cast<float>(n->duration * pixelsPerSecond);
        const float noteHeight = getKeyHeight();

        const float noteXPos = static_cast<float>(n->startTime * pixelsPerSecond);
        const float noteYPos = static_cast<float>(getHeight()) - ((n->noteNumber) * noteHeight);

        Rectangle<float> bounds(noteXPos, noteYPos, jmax(3.0f, noteWidth), noteHeight - 1.0f);

        float maxDrumWidth = 0.01f * getParentComponent()->getWidth();

        float hue = static_cast<float>(n->instrument) / 127.0f;

        Colour color = Colour::fromHSV(hue, 1.0f, 1.0f, 1.0f).brighter();

        // Note fill
        
        if (n->isDrum)
        {
            g.setColour(color);

            const float thickness = jmax(2.0f, noteHeight * 0.18f);
            const float pad = jmax(1.0f, thickness * 0.5f);

            const float x1 = noteXPos - maxDrumWidth/2;
            const float y1 = bounds.getY() + pad;
            const float x2 = noteXPos + maxDrumWidth/2;
            const float y2 = bounds.getBottom() - pad;

            g.drawLine(x1, y1, x2, y2, thickness);
            g.drawLine(x2, y1, x1, y2, thickness);
        }
        else
        {
            g.setColour(color.withAlpha(0.75f));
            g.fillRect(bounds.toNearestInt());

            if ((noteWidth >= 5) && (noteHeight >= 8))
            {
                const float maxVelocityWidth = static_cast<float>(noteWidth - 4);
                const float verticalOffset = static_cast<float>(noteHeight) * 0.5f - 2.0f;
                const float velocityHeight = 4.0f;

                g.setColour(color.brighter());
                g.fillRect(bounds.translated(2, verticalOffset)
                               .withWidth(maxVelocityWidth * n->velocity / 127.0f)
                               .withHeight(velocityHeight)
                               .toNearestInt());
            }
        }
    }
}

void NoteGridComponent::setResolution(double pps)
{
    pixelsPerSecond = pps;

    updateSize();
}

void NoteGridComponent::setLength(double len)
{
    lengthInSeconds = len;

    updateSize();
}

void NoteGridComponent::updateSize()
{
    setSize(static_cast<int>(pixelsPerSecond * lengthInSeconds), getHeight());
}

void NoteGridComponent::insertNote(MidiNote n)
{
    MidiNote* note = new MidiNote(n);
    midiNotes.add(note);

    repaint();
}

void NoteGridComponent::resetNotes()
{
    for (int i = 0; i < midiNotes.size(); i++)
    {
        MidiNote* note = midiNotes.getReference(i);
        delete note;
    }

    midiNotes.clear();

    repaint();
}
