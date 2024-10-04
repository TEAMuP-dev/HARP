#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "../pianoroll/PianoRollComponent.hpp"
#include "MediaDisplayComponent.h"


class MidiDisplayComponent : public MediaDisplayComponent
{
public:

    MidiDisplayComponent();
    ~MidiDisplayComponent();

    static StringArray getSupportedExtensions();
    StringArray getInstanceExtensions() { return MidiDisplayComponent::getSupportedExtensions(); }

    void drawMainArea(Graphics& g, Rectangle<int>& a) override;
    void resized() override;

    float getMediaWidth() override;

    void loadMediaFile(const URL& filePath) override;

    void setPlaybackPosition(double t) override;
    double getPlaybackPosition() override;

    bool isPlaying() override;
    void startPlaying() override;
    void stopPlaying() override;

    double getTotalLengthInSecs() override { return totalLengthInSecs; }

    void updateVisibleRange(Range<double> newRange) override;

    void addLabels(LabelList& labels) override;

private:

    double xToTime(const float x) override;
    float timeToX(const double t) override;

    void resetDisplay() override;

    void postLoadActions(const URL& filePath) override;

    double totalLengthInSecs;

    PianoRollComponent pianoRoll{70, scrollBarSize, spacing};
};
