// Adapted from https://github.com/Sjhunt93/Piano-Roll-Editor

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

#include "KeyboardComponent.hpp"

using namespace juce;

struct MidiNote
{
public:
    unsigned char noteNumber;
    double startTime;
    double duration;
    unsigned char velocity;

    MidiNote(unsigned char n, double s, double d, unsigned char v)
        : noteNumber(n), startTime(s), duration(d), velocity(v)
    {
    }

    MidiNote(const MidiNote& other)
    {
        noteNumber = other.noteNumber;
        startTime = other.startTime;
        duration = other.duration;
        velocity = other.velocity;
    }
};

class NoteGridComponent : public KeyboardComponent
{
public:
    NoteGridComponent();

    ~NoteGridComponent() override;

    void paint(Graphics& g) override;

    void setResolution(double pps);
    void setLength(double len);

    double getPixelsPerSecond() { return pixelsPerSecond; }
    double getLengthInSeconds() { return lengthInSeconds; }

    void updateSize();

    void insertNote(MidiNote n);
    void resetNotes();

    bool isKeyboardComponent() override { return false; }

private:
    Array<MidiNote*> midiNotes;

    double pixelsPerSecond;
    double lengthInSeconds;
};
