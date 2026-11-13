/**
 * @file GeneralSettingsTab.h
 * @brief Tab with general settings and maintenance actions.
 * @author saumya-pailwan, cwitkowitz, lindseydeng, huiranyu
 */

#pragma once

#include <JuceHeader.h>

#include "../../clients/Client.h"

#include "../../gui/Palette.h"

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

    // Space around the contents, and between a row of buttons and what follows it
    static constexpr int pagePadding = 14;
    static constexpr int buttonHeight = 28;
    static constexpr int buttonGap = 8;
    static constexpr int sectionGap = 12;

    ModelStyle::SectionHeader logHeader { "Log" };
    TextButton openLogFolderButton;
    TextButton clearLogsButton;

    ModelStyle::SectionHeader settingsHeader { "Settings" };
    TextButton openSettingsButton;
    TextButton restoreDefaultsButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GeneralSettingsTab)
};
