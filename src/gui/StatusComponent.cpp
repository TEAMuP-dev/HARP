#include "StatusComponent.h"

InstructionBox::InstructionBox(float fontSize, juce::Justification justification)
{
    statusLabel.setJustificationType(justification);
    statusLabel.setFont(fontSize);
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xE0, 0xE0, 0xE0));
    addAndMakeVisible(statusLabel);
}

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
    addAndMakeVisible(viewport);

    contentLabel.setJustificationType(juce::Justification::topLeft);
    contentLabel.setFont(fontSize);
    contentLabel.setColour(juce::Label::textColourId, juce::Colour(0xE0, 0xE0, 0xE0));
    contentLabel.setInterceptsMouseClicks(false, false);
    contentLabel.setMinimumHorizontalScale(1.0f);

    viewport.setViewedComponent(&contentLabel, false);
    viewport.setScrollBarsShown(true, false); // vertical yes, horizontal no
}

void StatusBox::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0x33, 0x33, 0x33));
    g.fillAll();
    g.setColour(juce::Colour(0x44, 0x44, 0x44));
    g.drawRect(getLocalBounds(), 1);
}

void StatusBox::resized()
{
    viewport.setBounds(getLocalBounds());

    contentLabel.setSize(
        viewport.getWidth() - viewport.getScrollBarThickness(),
        juce::jmax(contentLabel.getHeight(), viewport.getHeight())
    );
}

void StatusBox::appendRemoteMessage(const juce::String& level,
                                    const juce::String& message)
{
    juce::String prefix;

    if (level == "info")        prefix = "[info] ";
    else if (level == "warning") prefix = "[warning] ";
    else if (level == "error")   prefix = "[error] ";

    accumulatedMessages << prefix << message << "\n";

    contentLabel.setText(accumulatedMessages, juce::dontSendNotification);

    // Resize height to fit content
    contentLabel.setSize(
        viewport.getWidth() - viewport.getScrollBarThickness(),
        contentLabel.getTextHeight()
    );

    // Auto scroll to bottom
    viewport.setViewPosition(
        0,
        juce::jmax(0, contentLabel.getBottom() - viewport.getMaximumVisibleHeight())
    );
    repaint();
}

void StatusBox::setStatusMessage(const juce::String& message)
{
    accumulatedMessages = message;
    contentLabel.setText(message, juce::dontSendNotification);

    contentLabel.setSize(
        viewport.getWidth() - viewport.getScrollBarThickness(),
        contentLabel.getTextHeight()
    );
}

void StatusBox::clearStatusMessage()
{
    accumulatedMessages.clear();
    contentLabel.setText({}, juce::dontSendNotification);
}
