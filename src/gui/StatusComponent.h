/**
 * @file StatusComponent.h
 * @brief Instructions and status components for the GUI
 * @author xribene
 * 
 * Both are identical. The reason is in order for them to be sharedResources
 * they can't inherit from the same class. This is a workaround.
 * There is probably a better way to do this.
 */
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
