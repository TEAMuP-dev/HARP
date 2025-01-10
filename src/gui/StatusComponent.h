#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

class InstructionBox : public juce::Component
{
public:
    InstructionBox(float fontSize = 15.0f,
                   juce::Justification justification = juce::Justification::centred);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setStatusMessage(const juce::String& message);
    void clearStatusMessage();

protected:
    juce::Label statusLabel;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstructionBox)
};

class StatusBox : public juce::Component
{
public:
    StatusBox(float fontSize = 15.0f,
              juce::Justification justification = juce::Justification::centred);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setStatusMessage(const juce::String& message);
    void clearStatusMessage();

protected:
    juce::Label statusLabel;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StatusBox)
};
