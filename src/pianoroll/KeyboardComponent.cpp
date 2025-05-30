// Adapted from https://github.com/Sjhunt93/Piano-Roll-Editor

#include "KeyboardComponent.hpp"

const char* KeyboardComponent::pitchNames[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
};

const Array<int> KeyboardComponent::blackPitches = Array(1, 3, 6, 8, 10);

KeyboardComponent::KeyboardComponent() {}

KeyboardComponent::~KeyboardComponent() {}

float KeyboardComponent::getKeyHeight() { return getHeight() / 128.0; }

void KeyboardComponent::paint(Graphics& g)
{
    const float keyHeight = getKeyHeight();

    float cumHeight = 0; // Cumulative height

    for (int i = 127; i >= 0; i--)
    {
        const int pitch = i % 12;
        const int octave = i / 12 - 1;

        Colour blackKeyColor;
        Colour whiteKeyColor;

        if (isKeyboardComponent())
        {
            blackKeyColor = Colours::darkgrey;
            whiteKeyColor = Colours::lightgrey.darker();
        }
        else
        {
            blackKeyColor = Colours::darkgrey.brighter(0.2f).withAlpha(0.5f);
            whiteKeyColor = Colours::lightgrey.darker().withAlpha(0.5f);
        }

        g.setColour(blackPitches.contains(pitch) ? blackKeyColor : whiteKeyColor);
        g.fillRect(0, (int) cumHeight, getWidth(), (int) keyHeight);

        if (isKeyboardComponent())
        {
            String noteName = String(i) + " (" + pitchNames[pitch] + String(octave) + ")";

            g.setColour(Colours::white);
            g.drawText(
                noteName, 5, (int) cumHeight, getWidth(), (int) keyHeight, Justification::left);
        }

        cumHeight += keyHeight;

        g.setColour(Colours::black);
        g.drawLine(0, (int) cumHeight, getWidth(), (int) cumHeight);
    }
}
