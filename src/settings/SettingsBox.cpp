#include "SettingsBox.h"
#include "GeneralSettingsTab.h"
#include "AudioSettingsTab.h"

SettingsBox::SettingsBox()
    : tabComponent(juce::TabbedButtonBar::TabsAtTop)
{
    tabComponent.addTab("General", juce::Colours::lightgrey, new GeneralSettingsTab(), true);
    tabComponent.addTab("Audio",   juce::Colours::lightgrey, new AudioSettingsTab(), true);
    addAndMakeVisible(tabComponent);
    setSize(520, 360); 
}

void SettingsBox::resized()
{
    tabComponent.setBounds(getLocalBounds());
}
