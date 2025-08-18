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
    double getPixelsPerSecond() { return pixelsPerSecond; }

    void setLength(double len);
    double getLengthInSeconds() { return lengthInSeconds; }

    bool isKeyboardComponent() override { return false; }

    void updateSize();

    void insertNote(MidiNote n);
    void resetNotes();

private:
    Array<MidiNote*> midiNotes;

    double pixelsPerSecond;
    double lengthInSeconds;
};
