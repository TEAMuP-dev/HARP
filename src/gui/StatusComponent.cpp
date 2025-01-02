#include "StatusComponent.h"

// StatusComponent::StatusComponent(float fontSize, juce::Justification justification)
// {
//     statusLabel.setJustificationType(justification);
//     statusLabel.setFont(juce::Font(fontSize));
//     addAndMakeVisible(statusLabel);
// }

// void StatusComponent::paint(juce::Graphics& g)
// {
//     g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

//     float cornerSize = 10.0f;
//     auto bounds = getLocalBounds().toFloat();

//     g.setColour(juce::Colours::grey);
//     g.fillRoundedRectangle(bounds, cornerSize);
// }

// void StatusComponent::resized() { statusLabel.setBounds(getLocalBounds()); }

// void StatusComponent::setStatusMessage(const juce::String& message)
// {
//     statusLabel.setText(message, juce::NotificationType::dontSendNotification);
//     DBG("StatusComponent::setStatusMessage: " + message);
// }

// void StatusComponent::clearStatusMessage()
// {
//     statusLabel.setText("", juce::NotificationType::dontSendNotification);
// }

// BaseStatusBox implementations
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

// InstructionBox implementations
// InstructionBox::InstructionBox() {};
// InstructionBox::~InstructionBox() { clearSingletonInstance(); }
// JUCE_IMPLEMENT_SINGLETON(InstructionBox)
// InstructionBox::InstructionBox() : BaseStatusBox(15.0f, juce::Justification::centred)
// {
//     // Any specific initialization for InstructionBox
// }
// InstructionBox::~InstructionBox() { clearSingletonInstance(); }

// StatusBox implementations
// std::shared_ptr<StatusBox> StatusBox::instance = nullptr;

// std::shared_ptr<StatusBox> StatusBox::getInstance()
// {
//     if (!instance)
//         instance = std::shared_ptr<StatusBox>(new StatusBox);
//     return instance;
// }

// StatusBox::StatusBox() {};
// StatusBox::~StatusBox() { clearSingletonInstance(); }
// JUCE_IMPLEMENT_SINGLETON(StatusBox)
// StatusBox::StatusBox() : BaseStatusBox(15.0f, juce::Justification::centred)
// {
//     // Any specific initialization for StatusBox
// }
// StatusBox::~StatusBox() { clearSingletonInstance(); }
