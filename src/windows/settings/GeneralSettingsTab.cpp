#include "GeneralSettingsTab.h"

GeneralSettingsTab::GeneralSettingsTab()
{
    // Set up button to open log folder
    openLogFolderButton.setButtonText("Open Log Folder");
    openLogFolderButton.onClick = [this] { handleOpenLogFolder(); };
    addAndMakeVisible(openLogFolderButton);

    // Set up button to open settings file
    openSettingsButton.setButtonText("Open Settings File");
    openSettingsButton.onClick = [this] { handleOpenSettings(); };
    addAndMakeVisible(openSettingsButton);
}

void GeneralSettingsTab::resized()
{
    Rectangle<int> area = getLocalBounds().reduced(10);

    openLogFolderButton.setBounds(area.removeFromTop(30));

    area.removeFromTop(10); // Filler space

    openSettingsButton.setBounds(area.removeFromTop(30));
}

void GeneralSettingsTab::handleOpenLogFolder()
{
    HARPLogger::getInstance()->getLogFile().revealToUser();
}

void GeneralSettingsTab::handleOpenSettings()
{
    if (auto* settings = Settings::getUserSettings())
    {
        settings->getFile().startAsProcess();
    }
    else
    {
        // TODO - handler error case
    }
}
