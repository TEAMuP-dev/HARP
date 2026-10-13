/**
 * @file StatusAreaWidget.h
 * @brief Defines shared resources and components for instructions and status.
 * @author cwitkowitz, saumya-pailwan, xribene
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../utils/Messages.h"

using namespace juce;

/**
 * Shows the latest instructions, at a smaller size if that is what it takes to fit the box
 * (down to a minimum), and cut short with an ellipsis if they do not fit even then.
 */
class InstructionsBox : public Component, ChangeListener
{
public:
    InstructionsBox() { sharedMessage->addChangeListener(this); }

    ~InstructionsBox() override { sharedMessage->removeChangeListener(this); }

    void paint(Graphics& g) override
    {
        g.setColour(Colour(0x33, 0x33, 0x33));
        g.fillAll();

        g.setColour(Colour(0x44, 0x44, 0x44));
        g.drawRect(getLocalBounds(), 1);

        layout.draw(g, layoutArea);

        if (lastLine.isNotEmpty())
        {
            g.setColour(textColour);
            g.setFont(lastLineFont);
            g.drawText(lastLine,
                       lastLineArea,
                       textJustification.getOnlyHorizontalFlags()
                           | Justification::verticallyCentred,
                       true);
        }
    }

    void resized() override { fitMessage(); }

    void changeListenerCallback(ChangeBroadcaster* /*source*/) override
    {
        message = sharedMessage->getMessage();

        fitMessage();
        repaint();
    }

private:
    TextLayout layOut(const String& text, float fontHeight, float width) const
    {
        AttributedString attributed;
        attributed.append(text, Font(FontOptions(fontHeight)), textColour);
        attributed.setJustification(textJustification.getOnlyHorizontalFlags());

        TextLayout result;
        result.createLayout(attributed, width);

        return result;
    }

    /* Lays the message out at the largest size, from maximumFontHeight down to
       minimumFontHeight, at which all of it fits. If it does not fit even at the smallest, the
       lines that fit are kept, and the last of them takes the rest of the message, cut short
       with an ellipsis. (A Label would draw text with line breaks past its bounds instead.) */
    void fitMessage()
    {
        const auto area = getLocalBounds().reduced(textInsetX, textInsetY).toFloat();

        layout = TextLayout();
        lastLine = {};

        if (message.isEmpty() || area.isEmpty())
            return;

        float fontHeight = maximumFontHeight;
        layout = layOut(message, fontHeight, area.getWidth());

        while (layout.getHeight() > area.getHeight() && fontHeight > minimumFontHeight)
        {
            fontHeight = jmax(minimumFontHeight, fontHeight - 0.5f);
            layout = layOut(message, fontHeight, area.getWidth());
        }

        if (layout.getHeight() <= area.getHeight())
        {
            layoutArea = area.withSizeKeepingCentre(area.getWidth(), layout.getHeight());
            return;
        }

        int numLines = 1;

        while (numLines < layout.getNumLines()
               && layout.getLine(numLines).getLineBoundsY().getEnd() <= area.getHeight())
        {
            ++numLines;
        }

        const auto& last = layout.getLine(numLines - 1);
        const int lastStart = last.stringRange.getStart();
        const auto lastBounds = last.getLineBoundsY();

        lastLine = message.substring(lastStart).replaceCharacters("\r\n", "  ").trim();
        lastLineFont = Font(FontOptions(fontHeight));
        lastLineArea =
            area.withTrimmedTop(lastBounds.getStart()).withHeight(lastBounds.getLength());

        // The lines before it break where they did, since they are laid out the same way
        layout = layOut(message.substring(0, lastStart).trimEnd(), fontHeight, area.getWidth());
        layoutArea = area;
    }

    static constexpr float maximumFontHeight = 15.0f;
    static constexpr float minimumFontHeight = 12.0f;
    static constexpr int textInsetX = 6;
    static constexpr int textInsetY = 4;

    const Justification textJustification { Justification::centred };
    const Colour textColour { 0xffe0e0e0 };

    SharedResourcePointer<InstructionsMessage> sharedMessage;
    String message;

    TextLayout layout;
    Rectangle<float> layoutArea;

    // The last line shown when the message is cut short, drawn with an ellipsis
    String lastLine;
    Font lastLineFont { FontOptions() };
    Rectangle<float> lastLineArea;
};

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
        // Transparent, so that the fill from paint() shows through as in the InstructionsBox
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
        /* Rebuild the full text on every change. The history is capped at
           StatusMessage::maxHistoryEntries short lines, so this is cheap, and
           it avoids incremental-append bookkeeping that can drop or duplicate
           entries when messages arrive from worker threads. */
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
