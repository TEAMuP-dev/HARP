/**
 * @file StatusAreaWidget.h
 * @brief Defines shared resources and components for instructions and status.
 * @author xribene, cwitkowitz
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

struct SharedMessage : public ChangeBroadcaster
{
    void setMessage(const String& m)
    {
        message = m;
        sendChangeMessage();
    }

    void clearMessage()
    {
        message.clear();
        sendChangeMessage();
    }

    String message;
};

struct StatusMessage : SharedMessage
{
};
struct InstructionsMessage : SharedMessage
{
};

template <typename MessageType>
class MessageBox : public Component, ChangeListener
{
public:
    MessageBox(float fontSize = 15.0f, Justification justification = Justification::centred)
    {
        messageLabel.setJustificationType(juce::Justification::topLeft);
        messageLabel.setFont(fontSize);
        messageLabel.setColour(juce::Label::textColourId, juce::Colour(0xE0, 0xE0, 0xE0));
        messageLabel.setInterceptsMouseClicks(false, false);
        messageLabel.setMinimumHorizontalScale(1.0f);

        viewport.setViewedComponent(&messageLabel, false);
        viewport.setScrollBarsShown(true, false); // vertical yes, horizontal no
        addAndMakeVisible(viewport);

        sharedMessage->addChangeListener(this);
    }

    ~MessageBox() override { sharedMessage->removeChangeListener(this); }

    void paint(Graphics& g)
    {
        g.setColour(Colour(0x33, 0x33, 0x33));
        g.fillAll();

        g.setColour(Colour(0x44, 0x44, 0x44));
        g.drawRect(getLocalBounds(), 1);
    }

    void resized()
    {
        viewport.setBounds(getLocalBounds());

        messageLabel.setSize(viewport.getWidth() - viewport.getScrollBarThickness(),
                             jmax(messageLabel.getHeight(), viewport.getHeight()));
    }

    void changeListenerCallback(ChangeBroadcaster* /*source*/)
    {
        messageLabel.setText(sharedMessage->message, dontSendNotification);
    }

private:
    SharedResourcePointer<MessageType> sharedMessage;
    Viewport viewport;
    Label messageLabel;
};

/*
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
    statusLabel.setText(message, juce::dontSendNotification);
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
*/

using StatusBox = MessageBox<StatusMessage>;
using InstructionsBox = MessageBox<InstructionsMessage>;

class StatusAreaWidget : public Component
{
public:
    StatusAreaWidget()
    {
        addAndMakeVisible(instructionsBox);
        addAndMakeVisible(statusBox);
    }

    ~StatusAreaWidget() {}

    void resized() override
    {
        FlexBox statusArea;
        statusArea.flexDirection = FlexBox::Direction::row;

        statusArea.items.add(FlexItem(instructionsBox).withFlex(1).withMargin(marginSize));
        statusArea.items.add(FlexItem(statusBox).withFlex(1).withMargin(marginSize));

        statusArea.performLayout(getLocalBounds());
    }

private:
    const float marginSize = 2;

    InstructionsBox instructionsBox;
    StatusBox statusBox;
};
