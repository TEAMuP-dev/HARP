/**
 * @file GeneralSettingsTab.h
 * @brief Placeholder tab for general settings.
 * @author lindseydeng
 */

#pragma once

#include <JuceHeader.h>

#include "../../utils/Logging.h"
#include "../../utils/Settings.h"

using namespace juce;

class GeneralSettingsTab : public Component
{
public:
    GeneralSettingsTab();
    ~GeneralSettingsTab() override = default;

    void resized() override;

private:
    void handleOpenLogFolder();
    void handleOpenSettings();
    void handleRestoreDefaultSettings();

    TextButton openLogFolderButton;
    TextButton openSettingsButton;
    TextButton restoreDefaultSettingsButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GeneralSettingsTab)
};
