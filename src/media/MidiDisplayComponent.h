#pragma once

#include "../pianoroll/PianoRollComponent.hpp"
#include "../pianoroll/SynthAudioSource.h"
#include "MediaDisplayComponent.h"

class MidiDisplayComponent : public MediaDisplayComponent
{
public:
    MidiDisplayComponent();
    MidiDisplayComponent(String trackName, bool required = true);
    ~MidiDisplayComponent() override;

    static StringArray getSupportedExtensions();
    StringArray getInstanceExtensions() override
    {
        return MidiDisplayComponent::getSupportedExtensions();
    }

    void repositionOverheadPanel() override;
    // void repositionContent() override; // new from v2
    void repositionScrollBar() override;

    Component* getMediaComponent() override { return pianoRoll.getNoteGrid(); }

    float getMediaXPos() override
    {
        return static_cast<float>(pianoRoll.getKeyboardWidth() + pianoRoll.getPianoRollSpacing());
    }

    void loadMediaFile(const URL& filePath) override;

    void startPlaying() override;

    double getTotalLengthInSecs() override { return totalLengthInSecs; }
    float getPixelsPerSecond() override { return (float) pianoRoll.getResolution(); }

    void updateVisibleRange(Range<double> newRange) override;

    // void addLabels(LabelList& labels) override; // was deleted in new v2

    void resized() override;

    bool shouldRenderLabel(const std::unique_ptr<OutputLabel>& label) const override
    {
        return dynamic_cast<MidiLabel*>(label.get()) != nullptr;
    }

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
