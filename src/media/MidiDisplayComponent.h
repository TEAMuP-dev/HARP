#pragma once

#include "../pianoroll/PianoRollComponent.hpp"
#include "../pianoroll/SynthAudioSource.h"
#include "MediaDisplayComponent.h"

class MidiDisplayComponent : public MediaDisplayComponent
{
public:
    MidiDisplayComponent();
    MidiDisplayComponent(String trackName);
    ~MidiDisplayComponent();

    static StringArray getSupportedExtensions();
    StringArray getInstanceExtensions() { return MidiDisplayComponent::getSupportedExtensions(); }

    // void repositionContent() override;
    void repositionScrollBar() override;

    Component* getMediaComponent() { return pianoRoll.getNoteGrid(); }

    float getMediaXPos() override
    {
        return pianoRoll.getKeyboardWidth() + pianoRoll.getPianoRollSpacing();
    }

    void loadMediaFile(const URL& filePath) override;

    void startPlaying() override;

    double getTotalLengthInSecs() override { return totalLengthInSecs; }
    float getPixelsPerSecond() override { return pianoRoll.getResolution(); }

    void updateVisibleRange(Range<double> newRange) override;

    void addLabels(LabelList& labels) override;

    void resized() override;

private:
    void resetDisplay() override;

    void postLoadActions(const URL& filePath) override;

    void verticalMove(float deltaY);

    void verticalZoom(float deltaZoom, float scrollPosY);

    void mouseWheelMove(const MouseEvent&, const MouseWheelDetails& wheel) override;

    double totalLengthInSecs;

    SynthAudioSource synthAudioSource;

    int medianMidi;
    float stdDevMidi;

    PianoRollComponent pianoRoll { 70, 5, scrollBarSize, controlSpacing };
};
