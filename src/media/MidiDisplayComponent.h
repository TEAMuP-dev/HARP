#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "../pianoroll/PianoRollComponent.hpp"
#include "../synthesizer/SynthAudioSource.h"
#include "MediaDisplayComponent.h"


class MidiDisplayComponent : public MediaDisplayComponent
{
public:

    MidiDisplayComponent();
    ~MidiDisplayComponent();

    static StringArray getSupportedExtensions();
    StringArray getInstanceExtensions() { return MidiDisplayComponent::getSupportedExtensions(); }

    void repositionContent() override;
    void repositionScrollBar() override;

    Component* getMediaComponent() { return pianoRoll.getNoteGrid(); }

    float getMediaXPos() override { return pianoRoll.getKeyboardWidth() + pianoRoll.getPianoRollSpacing(); }

    void loadMediaFile(const URL& filePath) override;

    void setPlaybackPosition(double t) override { transportSource.setPosition(t); };
    double getPlaybackPosition() override { transportSource.getCurrentPosition(); };

    bool isPlaying() override { return transportSource.isPlaying(); };
    void startPlaying() override;
    void stopPlaying() override { transportSource.stop(); };

    double getTotalLengthInSecs() override { return totalLengthInSecs; }
    float getPixelsPerSecond() override { return pianoRoll.getResolution(); }

    void updateVisibleRange(Range<double> newRange) override;

    void addLabels(LabelList& labels) override;

private:

    void resetDisplay() override;

    void postLoadActions(const URL& filePath) override;

    double totalLengthInSecs;

    TimeSliceThread thread{ "MIDI File Thread" };

    AudioFormatManager formatManager;
    AudioDeviceManager deviceManager;

    AudioSourcePlayer sourcePlayer;
    AudioTransportSource transportSource;
    SynthAudioSource synthAudioSource;

    PianoRollComponent pianoRoll{70, 5, scrollBarSize, controlSpacing};
};
