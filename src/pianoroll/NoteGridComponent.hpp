// Adapted from https://github.com/Sjhunt93/Piano-Roll-Editor

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

#include "KeyboardComponent.hpp"

using namespace juce;

class MidiNoteComponent : public Component
{
public:
    MidiNoteComponent(unsigned char n, unsigned char v, double s, double d)
    {
        noteNumber = n;
        velocity = v;
        startTime = s;
        duration = d;
    }

    MidiNoteComponent(const MidiNoteComponent& other)
    {
        noteNumber = other.noteNumber;
        velocity = other.velocity;
        startTime = other.startTime;
        duration = other.duration;
    }

    void paint(Graphics& g)
    {
        g.fillAll(Colours::darkgrey);

        Colour red(252, 97, 92);

        if (getWidth() > 2)
        {
            g.setColour(red);

            g.fillRect(1, 1, getWidth() - 2, getHeight() - 2);
        }

        if (getWidth() > 10)
        {
            g.setColour(red.brighter());

            const int maxVelocityWidth = getWidth() - 10;
            const int verticalPosition = getHeight() * 0.5 - 2;

            g.drawLine(5,
                       verticalPosition,
                       maxVelocityWidth * (getVelocity() / 127.0),
                       verticalPosition,
                       4);
        }
    }

    unsigned char getNoteNumber() { return noteNumber; }
    unsigned char getVelocity() { return velocity; }
    double getStartTime() { return startTime; }
    double getNoteLength() { return duration; }

private:
    unsigned char noteNumber;
    unsigned char velocity;
    double startTime;
    double duration;
};

class NoteGridComponent : public KeyboardComponent
{
public:
    NoteGridComponent();

    ~NoteGridComponent();

    void setResolution(double pps);

    void updateLength(double l);

    void updateSize();
    void resized() override;

    void insertNote(MidiNoteComponent n);
    void resetNotes();

    int getPixelsPerSecond() { return pixelsPerSecond; }
    double getLengthInSeconds() { return lengthInSeconds; }

    bool isKeyboardComponent() override { return false; }

private:
    Array<MidiNoteComponent*> midiNotes;

    double pixelsPerSecond;
    double lengthInSeconds;
};
