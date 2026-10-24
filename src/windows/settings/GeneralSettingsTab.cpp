#include "GeneralSettingsTab.h"

#include "../../utils/ModelCatalog.h"

GeneralSettingsTab::GeneralSettingsTab(std::function<void()> onRestore)
    : onRestoreDefaults(std::move(onRestore))
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
        /* The API keys and the custom models are the user's own data rather than
           preferences, so they are kept. The catalog also holds the custom models in
           memory, and would write them back the next time one is added or removed. */
        const String apiKeyPrefix = String(SharedAPIKeys::settingsPrefix) + ".";

        /* Copied first, since getAllKeys() is the live list. Removing from it while
           iterating would shift each next key into the slot just visited, which skips
           every other key and leaves those settings in place. */
        const StringArray keys = settings->getAllProperties().getAllKeys();

        for (const auto& key : keys)
        {
            if (! key.startsWith(apiKeyPrefix) && key != ModelCatalog::customPathsKey)
                settings->removeValue(key);
        }

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
        "All settings have been restored to their defaults. API keys and custom models are "
        "kept.",
        "Ok");
}
