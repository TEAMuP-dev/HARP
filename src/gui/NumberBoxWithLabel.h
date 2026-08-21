/**
 * @file NumberBoxWithLabel.h
 * @brief Custom number box component with label.
 * @author cwitkowitz
 */

#pragma once

#include "ControlComponent.h"

using namespace juce;

class NumberBoxWithLabel : public ControlComponent
{
public:
    NumberBoxWithLabel(const String& labelText)
        : numberBox(Slider::IncDecButtons, Slider::TextBoxLeft)
    {
        label.setText(labelText, dontSendNotification);
        label.setJustificationType(Justification::centred);

        numberBox.setIncDecButtonsMode(Slider::incDecButtonsDraggable_Vertical);

        addAndMakeVisible(label);
        addAndMakeVisible(numberBox);
    }

    void resized() override
    {
        auto numberBoxArea = getLocalBounds();
        auto labelArea = numberBoxArea.removeFromTop(numberBoxArea.getHeight() / 3);

        label.setBounds(labelArea);
        numberBox.setBounds(numberBoxArea);
    }

    int getMinimumRequiredWidth() const override
    {
        const int labelWidth = getLabelWidth(label);
        return jmax(minNumberBoxBody, labelWidth + defaultPadding);
    }

    Slider& getNumberBox() { return numberBox; }

private:
    static constexpr int minNumberBoxBody = 100;

    Label label;
    Slider numberBox;
};
