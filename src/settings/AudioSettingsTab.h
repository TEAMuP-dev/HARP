#pragma once

#include <JuceHeader.h>

class AudioSettingsTab : public juce::Component
{
public:
    AudioSettingsTab();
    ~AudioSettingsTab() override = default;

    void resized() override;

private:
    juce::Label deviceInfoLabel;
    juce::TextButton openAudioSettingsButton;

    void handleOpenAudioSettings();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsTab)
};
