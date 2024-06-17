#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

class StatusComponent : public juce::Component
{
public:
    StatusComponent(
        float fontSize = 15.0f, 
        juce::Justification justification = juce::Justification::centred);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setStatusMessage(const juce::String& message);
    void clearStatusMessage();

private:
    juce::Label statusLabel;
};

