#pragma once

#include <JuceHeader.h>
#include "GeneralSettingsTab.h"
#include "AudioSettingsTab.h"
#include "LoginTab.h"

class SettingsBox : public juce::Component
{
public:
    SettingsBox();
    ~SettingsBox() override = default;

    void resized() override;

private:
    juce::TabbedComponent tabComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsBox)
};
