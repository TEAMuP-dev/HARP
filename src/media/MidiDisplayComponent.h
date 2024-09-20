#pragma once

#include "../pianoroll/PianoRollComponent.hpp"
#include "../synthesizer/SynthAudioSource.h"
#include "MediaDisplayComponent.h"


class MidiDisplayComponent : public MediaDisplayComponent
{
public:

    MidiDisplayComponent()
    {

        thread.startThread(Thread::Priority::normal);

        formatManager.registerBasicFormats();

        deviceManager.initialise(0, 2, nullptr, true, {}, nullptr);
        deviceManager.addAudioCallback(&sourcePlayer);

        sourcePlayer.setSource(&transportSource);

        addAndMakeVisible(pianoRoll);

        mediaHandlerInstructions = "MIDI pianoroll.\nClick and drag to start playback from any point in the pianoroll\nVertical scroll to zoom in/out.\nHorizontal scroll to move the pianoroll.";

        // transportSource.setSource(&synthAudioSource,
        //                           32768, // tells it to buffer this many samples ahead
        //                           &thread); // this is the background thread to use for reading-ahead
        transportSource.setSource(&synthAudioSource);
    }

    ~MidiDisplayComponent() {
        deviceManager.removeAudioCallback(&sourcePlayer);

        transportSource.setSource(nullptr);
        sourcePlayer.setSource(nullptr);
    }

    void drawMainArea(Graphics& g, Rectangle<int>& a) override
    {
        pianoRoll.setBounds(a);
    }

    void resized() override
    {
        Rectangle<int> scrollBarArea = getLocalBounds().removeFromBottom(scrollBarSize + 2 * scrollBarSpacing);
        scrollBarArea = scrollBarArea.removeFromRight(scrollBarArea.getWidth() - pianoRoll.getKeyboardWidth() - 5);
        scrollBarArea = scrollBarArea.removeFromLeft(scrollBarArea.getWidth() - 2 * pianoRoll.getScrollBarSize() - 4 * pianoRoll.getScrollBarSpacing());

        horizontalScrollBar.setBounds(scrollBarArea.reduced(scrollBarSpacing));
    }

    static StringArray getSupportedExtensions()
    {
        StringArray extensions;

        extensions.add(".mid");
        extensions.add(".midi");

        return extensions;
    }

    StringArray getInstanceExtensions()
    {
        return MidiDisplayComponent::getSupportedExtensions();
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

        pianoRoll.resizeNoteGrid(totalLengthInSecs);

        juce::MidiMessageSequence allTracks;

        for (int trackIdx = 0; trackIdx < midiFile.getNumTracks(); ++trackIdx) {
            const juce::MidiMessageSequence* constTrack = midiFile.getTrack(trackIdx);

            if (constTrack != nullptr) {
                allTracks.addSequence(*constTrack, 0.0);
                allTracks.updateMatchedPairs();
            }
        }

        for (int eventIdx = 0; eventIdx < allTracks.getNumEvents(); ++eventIdx) {
            const auto midiEvent = allTracks.getEventPointer(eventIdx);
            const auto& midiMessage = midiEvent->message;

            double startTime = midiEvent->message.getTimeStamp();

            DBG("Event " << eventIdx << " at " << startTime << ": " << midiMessage.getDescription());

            if (midiMessage.isNoteOn()) {
                int noteNumber = midiMessage.getNoteNumber();
                int velocity = midiMessage.getVelocity();
                int noteChannel = midiMessage.getChannel();

                double duration = 0;

                for (int offIdx = eventIdx + 1; offIdx < allTracks.getNumEvents(); ++offIdx) {
                    const auto offEvent = allTracks.getEventPointer(offIdx);

                    // Find the matching note off event
                    if (offEvent->message.isNoteOff() && offEvent->message.getNoteNumber() == noteNumber && offEvent->message.getChannel() == noteChannel) {
                        duration = (offEvent->message.getTimeStamp() - midiEvent->message.getTimeStamp());
                        break;
                    }
                }

                // Create a component for each for each note
                MidiNoteComponent n = MidiNoteComponent(noteNumber, velocity, startTime, duration);
                pianoRoll.insertNote(n);
            }
        }

        synthAudioSource.useSequence(allTracks);
    }

    void setPlaybackPosition(double t) override
    {
        transportSource.setPosition(t);
    }

    double getPlaybackPosition() override
    {
        return transportSource.getCurrentPosition();
    }

    void startPlaying() override
    {
        // TODO
        // AlertWindow::showMessageBoxAsync(
        //     AlertWindow::WarningIcon,
        //     "NotImplementedError",
        //     "MIDI playback has not yet been implemented."
        // );
        transportSource.start();
    }

    void stopPlaying() override
    {
        transportSource.stop();
    }

    bool isPlaying() override
    {
        return transportSource.isPlaying();
    }

    double getTotalLengthInSecs() override
    {
        return totalLengthInSecs;
    }

    void updateVisibleRange(Range<double> newRange) override
    {
        MediaDisplayComponent::updateVisibleRange(newRange);

        pianoRoll.updateVisibleMediaRange(newRange);
    }

private:

    double xToTime(const float x) const override
    {
        auto totalWidth = pianoRoll.getPianoRollWidth();
        auto totalLength = visibleRange.getLength();
        auto visibleStart = visibleRange.getStart();
        auto keyboardWidth = pianoRoll.getKeyboardWidth();

        double t = ((x - keyboardWidth) / totalWidth) * totalLength + visibleStart;

        return t;
    }

    float timeToX(const double t) const override
    {
        float x;

        auto totalLength = visibleRange.getLength();

        if (totalLength <= 0) {
            x = 0;
        } else {
            auto totalWidth = (float) pianoRoll.getPianoRollWidth();
            auto visibleStart = visibleRange.getStart();
            auto visibleOffset = (float) t - visibleStart;
            auto keyboardWidth = pianoRoll.getKeyboardWidth();

            x = totalWidth * visibleOffset / totalLength + keyboardWidth;
        }

        return x;
    }

    void resetDisplay() override
    {
        pianoRoll.resetNotes();
        pianoRoll.resizeNoteGrid(0.0);
    }

    void postLoadActions(const URL& filePath) override {}

    PianoRollComponent pianoRoll{70, scrollBarSize, scrollBarSpacing};

    TimeSliceThread thread{ "midi file preview" };
    
    AudioFormatManager formatManager;
    AudioDeviceManager deviceManager;

    AudioSourcePlayer sourcePlayer;
    AudioTransportSource transportSource;
    SynthAudioSource synthAudioSource;

    double totalLengthInSecs;
};
