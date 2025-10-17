#pragma once

#include "../WebModel.h"
#include "AudioSettingsTab.h"
#include "GeneralSettingsTab.h"
#include "LoginTab.h"
#include <JuceHeader.h>

class SettingsBox : public juce::Component
{
public:
    SettingsBox(WebModel* m);
    ~SettingsBox() override = default;

    void resized() override;

private:
    juce::TabbedComponent tabComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsBox)
};
