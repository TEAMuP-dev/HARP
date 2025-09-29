#pragma once

#include <JuceHeader.h>

class GeneralSettingsTab : public juce::Component
{
public:
    GeneralSettingsTab();
    ~GeneralSettingsTab() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    juce::TextButton openSettingsButton;
    juce::TextButton openLogFolderButton;
    void handleOpenLogFolder();
    void handleOpenSettings();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GeneralSettingsTab)
};
