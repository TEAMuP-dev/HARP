#include "AudioSettingsTab.h"

AudioSettingsTab::AudioSettingsTab()
{
    deviceInfoLabel.setText("Audio Settings Placeholder", juce::dontSendNotification);
    deviceInfoLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(deviceInfoLabel);

    openAudioSettingsButton.setButtonText("Configure Audio Device...");
    openAudioSettingsButton.onClick = [this] { handleOpenAudioSettings(); };
    addAndMakeVisible(openAudioSettingsButton);
}

void AudioSettingsTab::resized()
{
    auto area = getLocalBounds().reduced(10);
    deviceInfoLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);
    openAudioSettingsButton.setBounds(area.removeFromTop(30));
}

void AudioSettingsTab::handleOpenAudioSettings()
{
    // Placeholder: Open JUCE's built-in audio device selector (if needed later)
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Audio Settings",
        "Audio settings configuration will go here.");
}
