#pragma once

#include "MediaDisplayComponent.h"
#include "pianoRoll/PianoRollEditorComponent.hpp"


class MidiDisplayComponent : public MediaDisplayComponent
{
public:
    void setupDisplay()
    {
        pianoRollEditor.setup(10, 400, 10);
    }

    void setZoomFactor(float xScale, float yScale)
    {
        // TODO
    }

    void loadMediaFile(const URL& filePath)
    {
        // Create the local file this URL points to
        File file = filePath.getLocalFile();

        std::unique_ptr<juce::FileInputStream> fileStream(file.createInputStream());

        // Read the MIDI file from the File object
        MidiFile midiFile;
        PRESequence sequence;

        if (!midiFile.readFrom(*fileStream))
        {
            DBG("Failed to read MIDI data from file.");
        }

        double tickLength = midiFile.getTimeFormat() / 960.0; // based on MIDI PPQ

        for (int trackIdx = 0; trackIdx < midiFile.getNumTracks(); ++trackIdx)
        {
            const juce::MidiMessageSequence* constTrack = midiFile.getTrack(trackIdx);
            if (constTrack != nullptr)
            {
                juce::MidiMessageSequence track(*constTrack);
                track.updateMatchedPairs();

                
                DBG("Track " << trackIdx << " has " << track.getNumEvents() << " events.");

                // Example processing: iterating over messages in the sequence
                for (int eventIdx = 0; eventIdx < track.getNumEvents(); ++eventIdx)
                {
                    const auto midiEvent = track.getEventPointer(eventIdx);
                    const auto& midiMessage = midiEvent->message;

                    DBG("Event " << eventIdx << ": " << midiMessage.getDescription());
                    if (midiMessage.isNoteOn())
                    {
                        int noteNumber = midiMessage.getNoteNumber();
                        int velocity = midiMessage.getVelocity();
                        double startTime = midiEvent->message.getTimeStamp() * tickLength;

                        // Find the matching note off event
                        double noteLength = 0;
                        for (int offIdx = eventIdx + 1; offIdx < track.getNumEvents(); ++offIdx)
                        {
                            const auto offEvent = track.getEventPointer(offIdx);
                            if (offEvent->message.isNoteOff() && offEvent->message.getNoteNumber() == noteNumber)
                            {
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

    void togglePlay()
    {
        // TODO
    }

    bool isPlaying()
    {
        // TODO
    }

    double getCurrentPosition()
    {
        // TODO
    }

private:
    PianoRollEditorComponent pianoRollEditor;

    st_int tickTest;

    String mediaHandlerInstructions = "MIDI pianoroll.\nClick and drag to start playback from any point in the pianoroll\nVertical scroll to zoom in/out.\nHorizontal scroll to move the pianoroll.";
};
