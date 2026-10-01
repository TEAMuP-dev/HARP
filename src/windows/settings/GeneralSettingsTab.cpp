#include "GeneralSettingsTab.h"

GeneralSettingsTab::GeneralSettingsTab(std::function<void()> onRestoreDefaults)
    : onRestoreDefaults(std::move(onRestoreDefaults))
{
    // Set up button to open log folder
    openLogFolderButton.setButtonText("Open Log Folder");
    openLogFolderButton.onClick = [this] { handleOpenLogFolder(); };
    addAndMakeVisible(openLogFolderButton);

    // Set up button to open settings file
    openSettingsButton.setButtonText("Open Settings File");
    openSettingsButton.onClick = [this] { handleOpenSettings(); };
    addAndMakeVisible(openSettingsButton);

    // Set up button to clear logs
    clearLogsButton.setButtonText("Clear Logs");
    clearLogsButton.onClick = [this] { handleClearLogs(); };
    addAndMakeVisible(clearLogsButton);

    // Set up button to restore default settings
    restoreDefaultsButton.setButtonText("Restore Default Settings");
    restoreDefaultsButton.onClick = [this] { handleRestoreDefaults(); };
    addAndMakeVisible(restoreDefaultsButton);
}

void GeneralSettingsTab::resized()
{
    Rectangle<int> area = getLocalBounds().reduced(10);

    openLogFolderButton.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);

    clearLogsButton.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);

    openSettingsButton.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);

    restoreDefaultsButton.setBounds(area.removeFromTop(30));
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

void GeneralSettingsTab::handleClearLogs()
{
    OpResult result = HARPLogger::getInstance()->clearLog();

    if (result.failed())
    {
        AlertWindow::showMessageBoxAsync(
            AlertWindow::WarningIcon, "Clear Logs Failed", toUserMessage(result.getError()), "Ok");

        return;
    }

    AlertWindow::showMessageBoxAsync(
        AlertWindow::InfoIcon,
        "Logs Cleared",
        "All logs (main.log and launch.log) have been successfully cleared.",
        "Ok");
}

void GeneralSettingsTab::handleRestoreDefaults()
{
    bool saved = false;

    if (auto* settings = Settings::getUserSettings())
    {
        const StringArray allKeys = settings->getAllProperties().getAllKeys();

        // Preserve any API token keys so the user's credentials
        // are not wiped by a settings restore
        const String tokenKeyPrefix = sharedTokens->settingsPrefix + ".";

        StringPairArray tokenValues;
        for (const auto& key : allKeys)
        {
            if (key.startsWith(tokenKeyPrefix))
                tokenValues.set(key, settings->getValue(key));
        }

        for (const auto& key : allKeys)
            settings->removeValue(key);

        // Restore saved tokens
        const StringArray tokenKeys = tokenValues.getAllKeys();
        for (const auto& key : tokenKeys)
            settings->setValue(key, tokenValues[key]);

        saved = settings->saveIfNeeded();
    }

    if (onRestoreDefaults)
        onRestoreDefaults();

    if (! saved)
    {
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                         "Restore Defaults Failed",
                                         "The settings file could not be written.",
                                         "Ok");

        return;
    }

    AlertWindow::showMessageBoxAsync(
        AlertWindow::InfoIcon,
        "Settings Restored",
        "All settings (except API tokens) have been restored to their defaults.",
        "Ok");
}
