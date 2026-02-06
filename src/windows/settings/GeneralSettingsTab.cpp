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

    // Set up button to restore default settings
    restoreDefaultSettingsButton.setButtonText("Restore Default Settings");
    restoreDefaultSettingsButton.onClick = [this] { handleRestoreDefaultSettings(); };
    addAndMakeVisible(restoreDefaultSettingsButton);
}

void GeneralSettingsTab::resized()
{
    Rectangle<int> area = getLocalBounds().reduced(10);

    openLogFolderButton.setBounds(area.removeFromTop(30));

    area.removeFromTop(10); // Filler space

    openSettingsButton.setBounds(area.removeFromTop(30));

    area.removeFromTop(10); // Filler space

    restoreDefaultSettingsButton.setBounds(area.removeFromTop(30));
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

void GeneralSettingsTab::handleRestoreDefaultSettings()
{
    NativeMessageBox::showYesNoBox(
        AlertWindow::QuestionIcon,
        "Restore Default Settings",
        "Are you sure you want to restore default settings? This will delete your current settings "
        "file and reset all preferences to their defaults.",
        this,
        ModalCallbackFunction::create(
            [this](int result)
            {
                if (result == 1) // Yes
                {
                    if (auto* settings = Settings::getUserSettings())
                    {
                        // Delete the settings file
                        settings->getFile().deleteFile();

                        // Clear in-memory settings
                        settings->clear();

                        // Prevent saving on exit
                        Settings::setSaveOnExit(false);

                        NativeMessageBox::showMessageBoxAsync(
                            AlertWindow::InfoIcon,
                            "Settings Restored",
                            "Settings have been restored to defaults. It is recommended to restart "
                            "the application for all changes to take full effect.",
                            this);
                    }
                }
            }));
}
