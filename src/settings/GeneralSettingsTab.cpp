#include "GeneralSettingsTab.h"
#include "../AppSettings.h"
#include "../HarpLogger.h"

GeneralSettingsTab::GeneralSettingsTab()
{
    // Setup toggle button
    rememberTokenToggle.setButtonText("Remember Hugging Face token");
    rememberTokenToggle.setToggleState(
        AppSettings::getBoolValue("rememberToken", false),
        juce::NotificationType::dontSendNotification
    );
    rememberTokenToggle.onClick = [this] { handleToggleChanged(); };
    addAndMakeVisible(rememberTokenToggle);

    // Setup button to open log folder
    openLogFolderButton.setButtonText("Open Log Folder");
    openLogFolderButton.onClick = [this] { handleOpenLogFolder(); };
    addAndMakeVisible(openLogFolderButton);
}

void GeneralSettingsTab::resized()
{
    DBG("GeneralSettingsTab::resized()");
    auto area = getLocalBounds().reduced(10);
    rememberTokenToggle.setBounds(area.removeFromTop(30));
    area.removeFromTop(10); // Spacer
    openLogFolderButton.setBounds(area.removeFromTop(30));
}

void GeneralSettingsTab::paint(juce::Graphics& g)
{
    DBG("GeneralSettingsTab::paint()");
    g.fillAll(juce::Colours::lightgrey);  
}

void GeneralSettingsTab::handleToggleChanged()
{
    bool remember = rememberTokenToggle.getToggleState();
    AppSettings::setValue("rememberToken", remember);
    AppSettings::saveIfNeeded();
}

void GeneralSettingsTab::handleOpenLogFolder()
{
    HarpLogger::getInstance()->getLogFile().revealToUser();
}