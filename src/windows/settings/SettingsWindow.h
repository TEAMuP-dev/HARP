/**
 * @file SettingsWindow.h
 * @brief Window containing tabbed settings for HARP.
 * @author cwitkowitz, lindseydeng
 */

#pragma once

#include <JuceHeader.h>

#include "GeneralSettingsTab.h"
#include "LoginTab.h"

#include "../../gui/TabBarLookAndFeel.h"

#include "../../utils/Interface.h"

using namespace juce;

class SettingsWindow : public Component
{
public:
    explicit SettingsWindow(std::function<void()> onRestoreDefaults = {})
        : tabComponent(TabbedButtonBar::TabsAtTop)
    {
        // The tabs and background of the main window
        const Colour background =
            getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground);

        tabComponent.getTabbedButtonBar().setLookAndFeel(&tabsLookAndFeel);
        tabComponent.setColour(TabbedComponent::backgroundColourId, background);
        tabComponent.setOutline(0);

        tabComponent.addTab("General", background, new GeneralSettingsTab(onRestoreDefaults), true);

        // Recorded as the tab is added rather than assumed, so that reordering the tabs
        // cannot leave this addressing the wrong one
        apiKeysTab = new LoginTab();
        apiKeysTabIndex = tabComponent.getNumTabs();

        tabComponent.addTab("API Keys", background, apiKeysTab, true);
        addAndMakeVisible(tabComponent);

        setSize(windowWidth, windowHeight);
    }

    ~SettingsWindow() override { tabComponent.getTabbedButtonBar().setLookAndFeel(nullptr); }

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
    static constexpr int windowWidth = 480;
    static constexpr int windowHeight = 300;

    // Declared before the tabs, which it has to outlive
    TabBarLookAndFeel tabsLookAndFeel;

    TabbedComponent tabComponent;

    // Owned by the TabbedComponent, held here only to address it
    LoginTab* apiKeysTab = nullptr;
    int apiKeysTabIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsWindow)
};
