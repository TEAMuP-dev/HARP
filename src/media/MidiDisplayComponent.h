#pragma once

#include "../pianoRoll/PianoRollEditorComponent.hpp"
#include "MediaDisplayComponent.h"


class MidiDisplayComponent : public MediaDisplayComponent
{
public:

    MidiDisplayComponent()
    {
        // 10 measures, 400 pixels per measure (width), and 10 pixels per note (height)
        // TODO - call again when new file is loaded?
        pianoRollEditor.setup(10, 400, 10);
        pianoRollEditor.setPlaybackMarkerPosition(0, false);
        addAndMakeVisible(pianoRollEditor);

        mediaHandlerInstructions = "MIDI pianoroll.\nClick and drag to start playback from any point in the pianoroll\nVertical scroll to zoom in/out.\nHorizontal scroll to move the pianoroll.";
    }

    ~MidiDisplayComponent()
    {
        // TODO
    }

    void drawMainArea(Graphics& g, Rectangle<int>& a) override
    {
        pianoRollEditor.setBounds(a);
    }

    static StringArray getSupportedExtensions()
    {
        StringArray extensions;

        extensions.add(".mid");
        extensions.add(".midi");

        return extensions;
    }

    void loadMediaFile(const URL& filePath) override
    {
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

                for (int eventIdx = 0; eventIdx < track.getNumEvents(); ++eventIdx) {
                    const auto midiEvent = track.getEventPointer(eventIdx);
                    const auto& midiMessage = midiEvent->message;

                    double startTime = midiEvent->message.getTimeStamp() * tickLength;

                    DBG("Event " << eventIdx << " at " << startTime << ": " << midiMessage.getDescription());

                    if (midiMessage.isNoteOn()) {
                        int noteNumber = midiMessage.getNoteNumber();
                        int velocity = midiMessage.getVelocity();

                        double noteLength = 0;

                        for (int offIdx = eventIdx + 1; offIdx < track.getNumEvents(); ++offIdx) {
                            const auto offEvent = track.getEventPointer(offIdx);

                            // Find the matching note off event
                            if (offEvent->message.isNoteOff() && offEvent->message.getNoteNumber() == noteNumber) {
                                noteLength = (offEvent->message.getTimeStamp() - midiEvent->message.getTimeStamp()) * tickLength;
                                break;
                            }
                        }

                        // Create a NoteModel for each event
                        NoteModel::Flags flags;
                        NoteModel noteModel(noteNumber, velocity, static_cast<st_int>(startTime), static_cast<st_int>(noteLength), flags);
                        sequence.events.push_back(noteModel);
                    }
                }
            }
        }

        pianoRollEditor.loadSequence(sequence);

        midiFile.convertTimestampTicksToSeconds();
        totalLengthInSecs = midiFile.getLastTimestamp();

        DBG("Total duration of MIDI file " << totalLengthInSecs << " seconds.");
    }

    void setPlaybackPosition(double t) override
    {
        // TODO
    }

    double getPlaybackPosition() override
    {
        // TODO
        return 0.0;
    }

    void startPlaying() override
    {
        // TODO
    }

    void stopPlaying() override
    {
        // TODO
    }

    bool isPlaying() override
    {
        // TODO
        return false;
    }

    double getTotalLengthInSecs() override
    {
        return totalLengthInSecs;
    }

private:

    double xToTime(const float x) const override
    {
        // TODO
        return 0.0;
    }

    float timeToX(const double t) const override
    {
        // TODO
        return 0.0f;
    }

    void resetDisplay() override
    {
        // TODO
    }

    void postLoadActions(const URL& filePath) override
    {
        // TODO
    }

    PianoRollEditorComponent pianoRollEditor;

    double totalLengthInSecs;
};
