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
    SettingsWindow() : tabComponent(TabbedButtonBar::TabsAtTop)
    {
        tabComponent.addTab("General", Colours::darkgrey, new GeneralSettingsTab(), true);
        tabComponent.addTab("API Keys", Colours::darkgrey, new LoginTab(), true);
        //tabComponent.addTab("Audio", Colours::darkgrey, new AudioSettingsTab(), true);
        addAndMakeVisible(tabComponent);

        setSize(400, 300);
    }

    ~SettingsWindow() override = default;

    void resized() override { tabComponent.setBounds(getLocalBounds()); }

private:
    TabbedComponent tabComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsWindow)
};
