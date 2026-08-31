/**
 * @file TextBoxWithLabel.h
 * @brief Custom text box component with label.
 * @author xribene
 */

#pragma once

#include "ControlComponent.h"

using namespace juce;

class TextBoxWithLabel : public ControlComponent
{
public:
    TextBoxWithLabel(const String& labelText)
    {
        label.setText(labelText, dontSendNotification);
        label.setJustificationType(Justification::centred);

        textBox.setMultiLine(true, true);
        textBox.setReadOnly(false);
        textBox.setCaretVisible(true);
        textBox.setScrollbarsShown(true);
        textBox.setWantsKeyboardFocus(true);

        /*
          Without this the parent takes the focus
          and it's not possible to type in the text box
        */
        setWantsKeyboardFocus(false);

        textBox.setInterceptsMouseClicks(true, false);
        textBox.setMouseClickGrabsKeyboardFocus(true);

        addAndMakeVisible(label);
        addAndMakeVisible(textBox);
    }

    void resized() override
    {
        auto textBoxArea = getLocalBounds();
        auto labelArea = textBoxArea.removeFromTop((int) (textBoxArea.getHeight() * 0.25f));

        label.setBounds(labelArea);
        textBox.setBounds(textBoxArea);
    }

    int getPreferredWidth() const override { return preferredTextBoxWidth; }

    int getPreferredHeight() const override { return preferredTextBoxHeight; }

    int getMinimumRequiredWidth() const override
    {
        const int labelWidth = getLabelWidth(label);
        return jmax(minTextBoxWidth, labelWidth + defaultPadding);
    }

    TextEditor& getTextBox() { return textBox; }

    void setText(const String& text) { textBox.setText(text, dontSendNotification); }

private:
    static constexpr int preferredTextBoxWidth = 200;
    static constexpr int preferredTextBoxHeight = 84;

    static constexpr int minTextBoxWidth = 120;

    Label label;
    TextEditor textBox;
};
