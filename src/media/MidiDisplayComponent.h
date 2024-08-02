#pragma once

#include "../pianoroll/PianoRollComponent.hpp"
#include "MediaDisplayComponent.h"


class MidiDisplayComponent : public MediaDisplayComponent
{
public:

    MidiDisplayComponent()
    {
        addAndMakeVisible(pianoRoll);

        mediaHandlerInstructions = "MIDI pianoroll.\nClick and drag to start playback from any point in the pianoroll\nVertical scroll to zoom in/out.\nHorizontal scroll to move the pianoroll.";
    }

    ~MidiDisplayComponent()
    {
        // TODO
    }

    void drawMainArea(Graphics& g, Rectangle<int>& a) override
    {
        pianoRoll.setBounds(a);
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

        if (!midiFile.readFrom(*fileStream)) {
            DBG("Failed to read MIDI data from file.");
        }

        midiFile.convertTimestampTicksToSeconds();

        totalLengthInSecs = midiFile.getLastTimestamp();

        DBG("Total duration of MIDI file " << totalLengthInSecs << " seconds.");

        pianoRoll.setup(totalLengthInSecs);

        for (int trackIdx = 0; trackIdx < midiFile.getNumTracks(); ++trackIdx) {
            const juce::MidiMessageSequence* constTrack = midiFile.getTrack(trackIdx);

            if (constTrack != nullptr) {
                juce::MidiMessageSequence track(*constTrack);
                track.updateMatchedPairs();

                DBG("Track " << trackIdx << " has " << track.getNumEvents() << " events.");

                for (int eventIdx = 0; eventIdx < track.getNumEvents(); ++eventIdx) {
                    const auto midiEvent = track.getEventPointer(eventIdx);
                    const auto& midiMessage = midiEvent->message;

                    double startTime = midiEvent->message.getTimeStamp();

                    DBG("Event " << eventIdx << " at " << startTime << ": " << midiMessage.getDescription());

                    if (midiMessage.isNoteOn()) {
                        int noteNumber = midiMessage.getNoteNumber();
                        int velocity = midiMessage.getVelocity();

                        double duration = 0;

                        for (int offIdx = eventIdx + 1; offIdx < track.getNumEvents(); ++offIdx) {
                            const auto offEvent = track.getEventPointer(offIdx);

                            // Find the matching note off event
                            if (offEvent->message.isNoteOff() && offEvent->message.getNoteNumber() == noteNumber) {
                                duration = (offEvent->message.getTimeStamp() - midiEvent->message.getTimeStamp());
                                break;
                            }
                        }

                        // Create a component for each for each note
                        MidiNoteComponent n = MidiNoteComponent(noteNumber, velocity, startTime, duration);
                        pianoRoll.insertNote(n);
                    }
                }
            }
        }
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
        AlertWindow::showMessageBoxAsync(
            AlertWindow::WarningIcon,
            "NotImplementedError",
            "MIDI playback has not yet been implemented."
        );
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

    PianoRollComponent pianoRoll;

    double totalLengthInSecs;
};
