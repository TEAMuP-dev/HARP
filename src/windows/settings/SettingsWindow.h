/**
 * @file SettingsWindow.h
 * @brief Window containing tabbed settings for HARP.
 * @author lindseydeng
 */

#pragma once

#include <JuceHeader.h>

#include "AudioSettingsTab.h"
#include "GeneralSettingsTab.h"
#include "LoginTab.h"

using namespace juce;

class SettingsWindow : public Component
{
public:
    SettingsWindow(std::function<void()> onRestoreDefaults = {})
        : tabComponent(TabbedButtonBar::TabsAtTop)
    {
        tabComponent.addTab(
            "General", Colours::darkgrey, new GeneralSettingsTab(onRestoreDefaults), true);

        /* Recorded as the tab is added rather than assumed, so that reordering the
           tabs or restoring the one below cannot leave this addressing the wrong one. */
        apiKeysTab = new LoginTab();
        apiKeysTabIndex = tabComponent.getNumTabs();

        tabComponent.addTab("API Keys", Colours::darkgrey, apiKeysTab, true);
        //tabComponent.addTab("Audio", Colours::darkgrey, new AudioSettingsTab(), true);
        addAndMakeVisible(tabComponent);

        setSize(400, 300);
    }

    ~SettingsWindow() override = default;

    void resized() override { tabComponent.setBounds(getLocalBounds()); }

    void showAPIKeysFor(Provider provider)
    {
        tabComponent.setCurrentTabIndex(apiKeysTabIndex);

        if (apiKeysTab != nullptr)
        {
            apiKeysTab->showProvider(provider);
        }
    }

private:
    TabbedComponent tabComponent;

    // Owned by the TabbedComponent, held here only to address it
    LoginTab* apiKeysTab = nullptr;
    int apiKeysTabIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsWindow)
};
