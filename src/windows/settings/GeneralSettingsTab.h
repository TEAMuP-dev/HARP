/**
 * @file GeneralSettingsTab.h
 * @brief Placeholder tab for general settings.
 * @author lindseydeng
 */

#pragma once

#include <JuceHeader.h>

#include "../../clients/Client.h"
#include "../../utils/Logging.h"
#include "../../utils/Settings.h"

using namespace juce;

class GeneralSettingsTab : public Component
{
public:
    GeneralSettingsTab(std::function<void()> onRestoreDefaults = {});
    ~GeneralSettingsTab() override = default;

    void resized() override;

private:
    void handleOpenLogFolder();
    void handleOpenSettings();
    void handleClearLogs();
    void handleRestoreDefaults();

    std::function<void()> onRestoreDefaults;

    SharedResourcePointer<SharedAPIKeys> sharedTokens;

    TextButton openLogFolderButton;
    TextButton clearLogsButton;
    TextButton openSettingsButton;
    TextButton restoreDefaultsButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GeneralSettingsTab)
};
