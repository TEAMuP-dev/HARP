#include "StatusComponent.h"

StatusComponent::StatusComponent()
{
    addAndMakeVisible(statusLabel);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setFont(juce::Font(15.0f));
}

void StatusComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    float cornerSize = 10.0f; 
    auto bounds = getLocalBounds().toFloat(); 

    g.setColour(juce::Colours::grey); 
    g.fillRoundedRectangle(bounds, cornerSize);
}


void StatusComponent::resized()
{
    statusLabel.setBounds(getLocalBounds());
}

void StatusComponent::setStatusMessage(const juce::String& message)
{
    statusLabel.setText(message, juce::NotificationType::dontSendNotification);
    DBG("StatusComponent::setStatusMessage: " + message);
}

void StatusComponent::clearStatusMessage()
{
    statusLabel.setText("", juce::NotificationType::dontSendNotification);
}
