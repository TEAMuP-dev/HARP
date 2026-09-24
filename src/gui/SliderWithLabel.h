/**
 * @file SliderWithLabel.h
 * @brief Custom slider component with label.
 * @author xribene
 */

#pragma once

#include "ControlComponent.h"

using namespace juce;

class SliderWithLabel : public ControlComponent
{
public:
    SliderWithLabel(const String& labelText, Slider::SliderStyle style)
        : slider(style, Slider::TextBoxBelow)
    {
        label.setText(labelText, dontSendNotification);
        label.setJustificationType(Justification::centred);

        slider.setColour(Slider::thumbColourId, Colours::coral);

        addAndMakeVisible(label);
        addAndMakeVisible(slider);
    }

    void resized() override
    {
        auto sliderArea = getLocalBounds();
        auto labelArea = sliderArea.removeFromTop(sliderArea.getHeight() / 6);

        label.setBounds(labelArea);
        slider.setBounds(sliderArea);
    }

    int getPreferredWidth() const override { return preferredSliderWidth; }

    int getPreferredHeight() const override { return preferredSliderHeight; }

    int getMinimumRequiredWidth() const override
    {
        const int labelWidth = getLabelWidth(label);
        return jmax(minSliderBody, labelWidth + defaultPadding);
    }

    Slider& getSlider() { return slider; }

private:
    static constexpr int preferredSliderWidth = 108;
    static constexpr int preferredSliderHeight = 108;

    static constexpr int minSliderBody = 60;

    Label label;
    Slider slider;
};
