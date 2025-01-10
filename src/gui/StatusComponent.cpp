#include "StatusComponent.h"

InstructionBox::InstructionBox(float fontSize, juce::Justification justification)
{
    statusLabel.setJustificationType(justification);
    statusLabel.setFont(fontSize);
    addAndMakeVisible(statusLabel);
}

void InstructionBox::paint(juce::Graphics& g) { g.fillAll(juce::Colours::lightgrey); }

void InstructionBox::resized() { statusLabel.setBounds(getLocalBounds()); }

void InstructionBox::setStatusMessage(const juce::String& message)
{
    statusLabel.setText(message, juce::dontSendNotification);
}

void InstructionBox::clearStatusMessage() { statusLabel.setText({}, juce::dontSendNotification); }

StatusBox::StatusBox(float fontSize, juce::Justification justification)
{
    statusLabel.setJustificationType(justification);
    statusLabel.setFont(fontSize);
    addAndMakeVisible(statusLabel);
}

void StatusBox::paint(juce::Graphics& g) { g.fillAll(juce::Colours::lightgrey); }

void StatusBox::resized() { statusLabel.setBounds(getLocalBounds()); }

void StatusBox::setStatusMessage(const juce::String& message)
{
    statusLabel.setText(message, juce::dontSendNotification);
}

void StatusBox::clearStatusMessage() { statusLabel.setText({}, juce::dontSendNotification); }
