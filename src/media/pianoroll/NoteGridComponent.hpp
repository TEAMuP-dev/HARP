/**
 * @file NoteGridComponent.hpp
 * @brief Component with rows of piano keys and MIDI notes.
 * @author cwitkowitz, derekllanes, xribene
 */

#pragma once

#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "KeyboardComponent.hpp"

using namespace juce;

struct MidiNote
{
public:
    MidiNote(unsigned char n,
             double s,
             double d,
             unsigned char v,
             unsigned char i = 0,
             bool drum = false)
        : noteNumber(n), startTime(s), duration(d), velocity(v), instrument(i), isDrum(drum)
    {
    }

    unsigned char noteNumber;
    double startTime;
    double duration;
    unsigned char velocity;
    unsigned char instrument;
    bool isDrum;
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

    bool isKeyboardComponent() override { return false; }

    void updateSize();

    void insertNote(MidiNote n);
    void resetNotes();

private:
    std::vector<MidiNote> midiNotes;

    double pixelsPerSecond;
    double lengthInSeconds;
};
