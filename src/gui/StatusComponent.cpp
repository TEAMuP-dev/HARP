#include "StatusComponent.h"

InstructionBox::InstructionBox(float fontSize, juce::Justification justification)
{
    statusLabel.setJustificationType(justification);
    statusLabel.setFont(fontSize);
    // statusLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xE0, 0xE0, 0xE0));
    addAndMakeVisible(statusLabel);
}

// void InstructionBox::paint(juce::Graphics& g) { g.fillAll(juce::Colours::lightgrey); }
void InstructionBox::paint(juce::Graphics& g)
{
    // Option 1: Dark theme
    g.setColour(juce::Colour(0x33, 0x33, 0x33));
    g.fillAll();
    g.setColour(juce::Colour(0x44, 0x44, 0x44));
    g.drawRect(getLocalBounds(), 1);
}
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
    // statusLabel.setColour(juce::Label::textColourId, juce::Colours::black);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xE0, 0xE0, 0xE0));
    addAndMakeVisible(statusLabel);
}

// void StatusBox::paint(juce::Graphics& g) { g.fillAll(juce::Colours::lightgrey); }
void StatusBox::paint(juce::Graphics& g)
{
    // Option 1: Dark theme
    g.setColour(juce::Colour(0x33, 0x33, 0x33));
    g.fillAll();
    g.setColour(juce::Colour(0x44, 0x44, 0x44));
    g.drawRect(getLocalBounds(), 1);
}
void StatusBox::resized() { statusLabel.setBounds(getLocalBounds()); }

void StatusBox::setStatusMessage(const juce::String& message)
{
    statusLabel.setText(message, juce::dontSendNotification);
}

void StatusBox::clearStatusMessage() { statusLabel.setText({}, juce::dontSendNotification); }
