/**
 * @file AudioSettingsTab.h
 * @brief Placeholder tab for audio settings.
 * @author lindseydeng
 */

#pragma once

#include <JuceHeader.h>

using namespace juce;

class AudioSettingsTab : public Component
{
public:
    AudioSettingsTab();
    ~AudioSettingsTab() override = default;

    void resized() override;

private:
    void handleOpenAudioSettings();

    Label deviceInfoLabel;
    TextButton openAudioSettingsButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettingsTab)
};
