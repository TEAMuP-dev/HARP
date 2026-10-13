/**
 * @file GeneralSettingsTab.h
 * @brief Tab with general settings and maintenance actions.
 * @author saumya-pailwan, cwitkowitz, lindseydeng, huiranyu
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
    explicit GeneralSettingsTab(std::function<void()> onRestore = {});
    ~GeneralSettingsTab() override = default;

    void resized() override;

private:
    void handleOpenLogFolder();
    void handleOpenSettings();
    void handleClearLogs();
    void handleRestoreDefaults();

    std::function<void()> onRestoreDefaults;

    TextButton openLogFolderButton;
    TextButton clearLogsButton;
    TextButton openSettingsButton;
    TextButton restoreDefaultsButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GeneralSettingsTab)
};
