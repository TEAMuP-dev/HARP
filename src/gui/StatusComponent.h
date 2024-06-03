#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

class StatusComponent : public juce::Component
{
public:
    StatusComponent();
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setStatusMessage(const juce::String& message);
    void clearStatusMessage();

private:
    juce::Label statusLabel;
};

