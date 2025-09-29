#include "GeneralSettingsTab.h"
#include "../AppSettings.h"
#include "../HarpLogger.h"

GeneralSettingsTab::GeneralSettingsTab()
{
    // Setup button to open log folder
    openLogFolderButton.setButtonText("Open Log Folder");
    openLogFolderButton.onClick = [this] { handleOpenLogFolder(); };
    addAndMakeVisible(openLogFolderButton);

    // Setup button to open settings file
    openSettingsButton.setButtonText("Open Settings File");
    openSettingsButton.onClick = [this] { handleOpenSettings();};
    addAndMakeVisible(openSettingsButton);
}

void GeneralSettingsTab::resized()
{
    DBG("GeneralSettingsTab::resized()");
    auto area = getLocalBounds().reduced(10);
    openLogFolderButton.setBounds(area.removeFromTop(30));
    area.removeFromTop(10); // Spacer
    openSettingsButton.setBounds(area.removeFromTop(30));
}

void GeneralSettingsTab::paint(juce::Graphics& g)
{
    DBG("GeneralSettingsTab::paint()");
    // g.fillAll(juce::Colours::lightgrey);  
}

void GeneralSettingsTab::handleOpenLogFolder()
{
    HarpLogger::getInstance()->getLogFile().revealToUser();
}

void GeneralSettingsTab::handleOpenSettings()
{
    AppSettings::getUserSettings()->getFile().startAsProcess();
}