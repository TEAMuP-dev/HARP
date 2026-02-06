/**
 * @file TextBoxWithLabel.h
 * @brief Custom text box component with label.
 * @author xribene
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

class TextBoxWithLabel : public Component
{
public:
    TextBoxWithLabel(const String& labelText)
    {
        label.setText(labelText, dontSendNotification);

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

    TextEditor& getTextBox() { return textBox; }

    void setText(const String& text) { textBox.setText(text, dontSendNotification); }

private:
    Label label;
    TextEditor textBox;
};
