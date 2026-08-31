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

        // Fixing the text box size keeps the buttons from growing taller than it
        numberBox.setTextBoxStyle(Slider::TextBoxLeft, false, textBoxWidth, rowHeight);

        addAndMakeVisible(label);
        addAndMakeVisible(numberBox);
    }

    void resized() override
    {
        auto area = getLocalBounds();

        if (area.isEmpty())
        {
            return;
        }

        label.setBounds(area.removeFromTop(jmin(labelHeight, area.getHeight())));

        /* JUCE sizes the increment / decrement buttons to fill whatever is left of
           the slider, so the slider is limited to one row and centered instead of
           being given the whole remaining area. */
        int boxHeight = jmin(rowHeight, area.getHeight());

        numberBox.setBounds(area.withSizeKeepingCentre(area.getWidth(), boxHeight));
    }

    int getPreferredWidth() const override { return preferredNumberBoxWidth; }

    int getPreferredHeight() const override { return preferredNumberBoxHeight; }

    int getMinimumRequiredWidth() const override
    {
        const int labelWidth = getLabelWidth(label);
        return jmax(minNumberBoxBody, labelWidth + defaultPadding);
    }

    Slider& getNumberBox() { return numberBox; }

private:
    static constexpr int preferredNumberBoxWidth = 140;
    static constexpr int preferredNumberBoxHeight = 56;

    static constexpr int minNumberBoxBody = 100;
    static constexpr int labelHeight = 20;
    static constexpr int rowHeight = 24;
    static constexpr int textBoxWidth = 64;

    Label label;
    Slider numberBox;
};
