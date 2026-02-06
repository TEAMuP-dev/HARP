#include "AudioSettingsTab.h"

AudioSettingsTab::AudioSettingsTab()
{
    deviceInfoLabel.setText("Audio Settings", dontSendNotification);
    deviceInfoLabel.setJustificationType(Justification::centredLeft);
    addAndMakeVisible(deviceInfoLabel);

    openAudioSettingsButton.setButtonText("Configure Audio Device");
    openAudioSettingsButton.onClick = [this] { handleOpenAudioSettings(); };
    addAndMakeVisible(openAudioSettingsButton);
}

void AudioSettingsTab::resized()
{
    Rectangle<int> area = getLocalBounds().reduced(10);

    deviceInfoLabel.setBounds(area.removeFromTop(30));

    area.removeFromTop(10); // Filler space

    openAudioSettingsButton.setBounds(area.removeFromTop(30));
}

void AudioSettingsTab::handleOpenAudioSettings()
{
    // Open JUCE's built-in audio device selector (placeholder)
    AlertWindow::showMessageBoxAsync(
        AlertWindow::InfoIcon, "Audio Settings", "Audio settings configuration will go here.");
}
