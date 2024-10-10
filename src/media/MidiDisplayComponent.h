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

    void paintMedia(Graphics& g, Rectangle<int>& a) override;

    void repositionScrollBar() override;

    Component* getMediaComponent() { return pianoRoll.getNoteGrid(); }

    //float getMediaStartXPos() override { return pianoRoll.getKeyboardWidth() + pianoRoll.getPianoRollSpacing(); }

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

    void resetDisplay() override;

    void postLoadActions(const URL& filePath) override;

    double totalLengthInSecs;

    PianoRollComponent pianoRoll{70, 5, scrollBarSize, spacing};
};
