#pragma once

#include "../pianoroll/PianoRollComponent.hpp"
#include "../pianoroll/SynthAudioSource.h"
#include "MediaDisplayComponent.h"

class MidiDisplayComponent : public MediaDisplayComponent
{
public:
    MidiDisplayComponent();
    MidiDisplayComponent(String name, bool req = true, DisplayMode mode = DisplayMode::Hybrid);
    ~MidiDisplayComponent() override;

    static StringArray getSupportedExtensions();
    StringArray getInstanceExtensions() override
    {
        return MidiDisplayComponent::getSupportedExtensions();
    }

    void resized() override;

    void changeListenerCallback(ChangeBroadcaster*) override { repositionLabels(); }

    void loadMediaFile(const URL& filePath) override;

    void startPlaying() override;

    double getTotalLengthInSecs() override { return totalLengthInSecs; }
    float getPixelsPerSecond() override { return static_cast<float>(pianoRoll.getResolution()); }

    void updateVisibleRange(Range<double> newRange) override;

    bool shouldRenderLabel(const std::unique_ptr<OutputLabel>& label) const override
    {
        return dynamic_cast<MidiLabel*>(label.get()) != nullptr;
    }

private:
    Component* getMediaComponent() override { return pianoRoll.getNoteGrid(); }

    float getMediaHeight() override;
    float getVerticalControlsWidth() override;

    float getMediaXPos() override;
    float mediaYToDisplayY(const float mY) override;

    void resetMedia() override;

    void postLoadActions(const URL& filePath) override;

    void verticalMove(float deltaY);

    void verticalZoom(float deltaZoom);

    void mouseWheelMove(const MouseEvent&, const MouseWheelDetails& wheel) override;

    double totalLengthInSecs = 0.0;

    SynthAudioSource synthAudioSource;

    int medianMidi;
    float stdDevMidi;

    PianoRollComponent pianoRoll {
        70, 3, scrollBarSize, controlSpacing, isThumbnailTrack(), isThumbnailTrack()
    };
};
