#include "NoteGridComponent.hpp"

NoteGridComponent::NoteGridComponent()
{
    pixelsPerSecond = 0.0;
    lengthInSeconds = 0.0;
}

NoteGridComponent::~NoteGridComponent() = default;

void NoteGridComponent::paint(Graphics& g)
{
    // Paint key background
    KeyboardComponent::paint(g);

    const float noteHeight = getKeyHeight();

    // A drum hit has no duration to speak of, so it is drawn as a cross of a fixed width
    const float drumWidth = 0.01f * static_cast<float>(getParentWidth());

    // Paint all notes
    for (const auto& n : midiNotes)
    {
        const float noteWidth = static_cast<float>(n.duration * pixelsPerSecond);

        const float noteXPos = static_cast<float>(n.startTime * pixelsPerSecond);
        /* The keyboard draws note 127 in the top row and note 0 in the bottom one, so the
           row of a note starts a row above where its number of rows from the bottom ends */
        const float noteYPos = static_cast<float>(getHeight()) - ((n.noteNumber + 1) * noteHeight);

        Rectangle<float> bounds(noteXPos, noteYPos, jmax(3.0f, noteWidth), noteHeight - 1.0f);

        float hue = static_cast<float>(n.instrument) / 127.0f;

        Colour color = Colour::fromHSV(hue, 1.0f, 1.0f, 1.0f).brighter();

        if (n.isDrum)
        {
            g.setColour(color);

            const float thickness = jmax(2.0f, noteHeight * 0.18f);
            const float pad = jmax(1.0f, thickness * 0.5f);

            const float x1 = noteXPos - drumWidth / 2.0f;
            const float y1 = bounds.getY() + pad;
            const float x2 = noteXPos + drumWidth / 2.0f;
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
                               .withWidth(maxVelocityWidth * n.velocity / 127.0f)
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
    midiNotes.push_back(n);

    repaint();
}

void NoteGridComponent::resetNotes()
{
    midiNotes.clear();

    repaint();
}
