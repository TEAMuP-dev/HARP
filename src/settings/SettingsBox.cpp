#include "SettingsBox.h"

SettingsBox::SettingsBox()
    : tabComponent(juce::TabbedButtonBar::TabsAtTop)
{
    tabComponent.addTab("General", juce::Colours::darkgrey, new GeneralSettingsTab(), true);
    tabComponent.addTab("Huggingface", juce::Colours::darkgrey, new LoginTab("huggingface"), true);
    tabComponent.addTab("Stability", juce::Colours::darkgrey, new LoginTab("stability"), true);
    // tabComponent.addTab("Audio", juce::Colours::darkgrey, new AudioSettingsTab(), true);
    addAndMakeVisible(tabComponent);
    setSize(400, 300); 
}

void SettingsBox::resized()
{
    tabComponent.setBounds(getLocalBounds());
}
