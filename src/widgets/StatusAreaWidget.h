/**
 * @file StatusAreaWidget.h
 * @brief Defines shared resources and components for instructions and status.
 * @author xribene, cwitkowitz
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../utils/Messages.h"

using namespace juce;

template <typename MessageType>
class MessageBox : public Component, ChangeListener
{
public:
    MessageBox(float fontSize = 15.0f, Justification justification = Justification::centred)
    {
        messageLabel.setFont(fontSize);
        messageLabel.setColour(Label::textColourId, Colour(0xE0, 0xE0, 0xE0));

        messageLabel.setJustificationType(justification);
        addAndMakeVisible(messageLabel);

        sharedMessage->addChangeListener(this);
    }

    ~MessageBox() override { sharedMessage->removeChangeListener(this); }

    void paint(Graphics& g) override
    {
        g.setColour(Colour(0x33, 0x33, 0x33));
        g.fillAll();

        g.setColour(Colour(0x44, 0x44, 0x44));
        g.drawRect(getLocalBounds(), 1);
    }

    void resized() override { messageLabel.setBounds(getLocalBounds()); }

    void changeListenerCallback(ChangeBroadcaster* /*source*/) override
    {
        messageLabel.setText(sharedMessage->getMessage(), dontSendNotification);
    }

private:
    SharedResourcePointer<MessageType> sharedMessage;
    Label messageLabel;
};

using StatusBox = MessageBox<StatusMessage>;
using InstructionsBox = MessageBox<InstructionsMessage>;

class StatusHistoryBox : public Component, ChangeListener
{
public:
    StatusHistoryBox()
    {
        historyEditor.setMultiLine(true);
        historyEditor.setReadOnly(true);
        historyEditor.setScrollbarsShown(true);
        historyEditor.setCaretVisible(false);
        historyEditor.setPopupMenuEnabled(false);
        // Transparent background so the parent's paint() fill shows through,
        // giving the same appearance as the InstructionsBox (which uses a plain Label).
        historyEditor.setColour(TextEditor::backgroundColourId, Colours::transparentBlack);
        historyEditor.setColour(TextEditor::textColourId, Colour(0xE0, 0xE0, 0xE0));
        historyEditor.setColour(TextEditor::outlineColourId, Colours::transparentBlack);
        historyEditor.setColour(TextEditor::focusedOutlineColourId, Colours::transparentBlack);

        addAndMakeVisible(historyEditor);

        historyEditor.setText(sharedMessage->getHistoryText(), false);
        historyEditor.moveCaretToEnd();

        sharedMessage->addChangeListener(this);
    }

    ~StatusHistoryBox() override { sharedMessage->removeChangeListener(this); }

    void paint(Graphics& g) override
    {
        g.setColour(Colour(0x33, 0x33, 0x33));
        g.fillAll();
        g.setColour(Colour(0x44, 0x44, 0x44));
        g.drawRect(getLocalBounds(), 1);
    }

    void resized() override { historyEditor.setBounds(getLocalBounds()); }

    void changeListenerCallback(ChangeBroadcaster* /*source*/) override
    {
        /* Rebuild the full text on every change: the history is capped at
           StatusMessage::maxHistoryEntries short lines, so this is cheap, and
           it avoids incremental-append bookkeeping that can drop or duplicate
           entries when messages arrive from worker threads */
        historyEditor.setText(sharedMessage->getHistoryText(), false);
        historyEditor.moveCaretToEnd();
    }

private:
    SharedResourcePointer<StatusMessage> sharedMessage;
    TextEditor historyEditor;
};

class StatusAreaWidget : public Component
{
public:
    StatusAreaWidget()
    {
        addAndMakeVisible(instructionsBox);
        addAndMakeVisible(statusHistoryBox);
    }

    ~StatusAreaWidget() override {}

    void resized() override
    {
        FlexBox statusArea;
        statusArea.flexDirection = FlexBox::Direction::row;

        statusArea.items.add(FlexItem(statusHistoryBox).withFlex(1).withMargin(marginSize));
        statusArea.items.add(FlexItem(instructionsBox).withFlex(1).withMargin(marginSize));

        statusArea.performLayout(getLocalBounds());
    }

private:
    const float marginSize = 2;

    InstructionsBox instructionsBox;
    StatusHistoryBox statusHistoryBox;
};
