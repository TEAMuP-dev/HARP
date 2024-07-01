#pragma once

#include "../pianoRoll/PianoRollEditorComponent.hpp"
#include "MediaDisplayComponent.h"


class MidiDisplayComponent : public MediaDisplayComponent
{
public:

    ~MidiDisplayComponent()
    {
        // TODO
    }

    void setupDisplay()
    {
        //default 10 bars/measures, with 900 pixels per bar (width) and 20 pixels per step (each note height)
        pianoRollEditor.setup(10, 400, 10);

        mediaHandlerInstructions = "MIDI pianoroll.\nClick and drag to start playback from any point in the pianoroll\nVertical scroll to zoom in/out.\nHorizontal scroll to move the pianoroll.";
    }

    void drawMainArea(Graphics& g, Rectangle<int>& a)
    {
        // TODO
    }

    static StringArray getSupportedExtensions()
    {
        StringArray extensions;

        extensions.add(".mid");
        extensions.add(".midi");

        return extensions;
    }

    void loadMediaFile(const URL& filePath)
    {
        MediaDisplayComponent::loadMediaFile(filePath);

        // Create the local file this URL points to
        File file = filePath.getLocalFile();

        std::unique_ptr<juce::FileInputStream> fileStream(file.createInputStream());

        // Read the MIDI file from the File object
        MidiFile midiFile;
        PRESequence sequence;

        if (!midiFile.readFrom(*fileStream)) {
            DBG("Failed to read MIDI data from file.");
        }

        double tickLength = midiFile.getTimeFormat() / 960.0; // based on MIDI PPQ

        for (int trackIdx = 0; trackIdx < midiFile.getNumTracks(); ++trackIdx) {
            const juce::MidiMessageSequence* constTrack = midiFile.getTrack(trackIdx);
            if (constTrack != nullptr) {
                juce::MidiMessageSequence track(*constTrack);
                track.updateMatchedPairs();


                DBG("Track " << trackIdx << " has " << track.getNumEvents() << " events.");

                // Example processing: iterating over messages in the sequence
                for (int eventIdx = 0; eventIdx < track.getNumEvents(); ++eventIdx) {
                    const auto midiEvent = track.getEventPointer(eventIdx);
                    const auto& midiMessage = midiEvent->message;

                    DBG("Event " << eventIdx << ": " << midiMessage.getDescription());
                    if (midiMessage.isNoteOn()) {
                        int noteNumber = midiMessage.getNoteNumber();
                        int velocity = midiMessage.getVelocity();
                        double startTime = midiEvent->message.getTimeStamp() * tickLength;

                        // Find the matching note off event
                        double noteLength = 0;
                        for (int offIdx = eventIdx + 1; offIdx < track.getNumEvents(); ++offIdx) {
                            const auto offEvent = track.getEventPointer(offIdx);
                            if (offEvent->message.isNoteOff() && offEvent->message.getNoteNumber() == noteNumber) {
                                noteLength = (offEvent->message.getTimeStamp() - midiEvent->message.getTimeStamp()) * tickLength;
                                break;
                            }
                        }
                        // Create a NoteModel for each midiEvent
                        NoteModel::Flags flags;
                        NoteModel noteModel(noteNumber, velocity, static_cast<st_int>(startTime), static_cast<st_int>(noteLength), flags);
                        sequence.events.push_back(noteModel);
                    }
                }
            }
        }

        pianoRollEditor.loadSequence(sequence);
    }

    void setPlaybackPosition(float x)
    {
        // TODO
    }

    float getPlaybackPosition()
    {
        // TODO
    }

    void startPlaying()
    {
        // TODO
    }

    void stopPlaying()
    {
        // TODO
    }

    bool isPlaying()
    {
        // TODO
    }

    double getTotalLengthInSecs()
    {
        // TODO
    }

private:

    void postLoadMediaActions(const URL& filePath)
    {
        // TODO
    }

    PianoRollEditorComponent pianoRollEditor;
};
