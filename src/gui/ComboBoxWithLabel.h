#pragma once
#include "juce_gui_basics/juce_gui_basics.h"

class ComboBoxWithLabel : public juce::Component
{
public:
    ComboBoxWithLabel(const juce::String& labelText = {})
    {
        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);

        addAndMakeVisible(label);
        addAndMakeVisible(comboBox);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto topArea = area.removeFromTop(20);
        label.setBounds(topArea);
        comboBox.setBounds(area);
    }

    juce::ComboBox& getComboBox() { return comboBox; }
    juce::Label& getLabel() { return label; }

    int getMinimumRequiredWidth() const
    {
        auto font = label.getFont();
        const int labelWidth    = font.getStringWidth(label.getText());
        const int padding       = 20;
        const int minComboWidth = 80;

        return juce::jmax(minComboWidth, labelWidth + padding);
    }

private:
    juce::Label label;
    juce::ComboBox comboBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ComboBoxWithLabel)
};
