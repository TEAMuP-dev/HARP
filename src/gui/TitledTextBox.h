

/**
 * @file
 * @brief Custom UI components for the HARPPlugin
 * @author xribene
 */

#pragma once

#include "juce_audio_basics/juce_audio_basics.h"

using namespace juce;

/**
 * @brief A class for defining the toolbar slider style.
 */
class TitledTextBox : public juce::Component
{
public:
    TitledTextBox()
    {
        // Attach the label to the text box
        // titleLabel.attachToComponent(&textBox, true);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        DBG("My bounds are " + getBounds().toString());
        auto topArea = area.removeFromTop((int)(area.getHeight() * 0.3));
        titleLabel.setBounds(topArea);
        textBox.setBounds(area);
        textBox.setMultiLine(true, true);
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(textBox);
    }

    void setName(const juce::String &nameId) override
    {
        textBox.setName(nameId);
    }

    juce::String getName()
    {
        return textBox.getName();
    }
    
    void setTitle(const juce::String &title)
    {
        titleLabel.setText(title, juce::dontSendNotification);
    }

    void setText(const juce::String &text)
    {
        textBox.setText(text, juce::dontSendNotification);
    }

    void addListener(juce::TextEditor::Listener *listener)
    {
        textBox.addListener(listener);
    }

    juce::String getText()
    {
        return textBox.getText();
    }

private:
    juce::TextEditor textBox;
    juce::Label titleLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TitledTextBox)
};