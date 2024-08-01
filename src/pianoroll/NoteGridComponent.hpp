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

        g.setColour(red);
        g.fillRect(1, 1, getWidth() - 2, getHeight() - 2);

        if (getWidth() > 10) {
            g.setColour(red.brighter());

            const int maxVelocityWidth = getWidth() - 10;
            const int verticalPosition = getHeight() * 0.5 - 2;

            g.drawLine(5, verticalPosition, maxVelocityWidth * (getVelocity() / 127.0), verticalPosition, 4);
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

    void updateLength(double l);

    void setResolution(float pps);

    void resized() override;

    void insertNotes(Array<MidiNoteComponent*> notes);
    void resetNotes();

    double getLengthInSeconds() { return lengthInSecs; }

private:

    Array<MidiNoteComponent*> midiNotes;

    float pixelsPerSecond;
    double lengthInSecs;
};
