/**
 * @file
 * @brief Custom UI components for the HARPPlugin
 * @author xribene
 */

#pragma once

#include "juce_audio_basics/juce_audio_basics.h"

using namespace juce;

class SliderWithLabel : public juce::Component
{
public:
    SliderWithLabel(const juce::String& labelText, juce::Slider::SliderStyle style)
        : slider(style, juce::Slider::TextBoxBelow)
    {
        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
        addAndMakeVisible(slider);
        setThumbColor(juce::Colours::coral);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        label.setBounds(bounds.removeFromTop(bounds.getHeight() / 6));
        slider.setBounds(bounds);
        // DBG("Slider bounds now considered " + getBounds().toString());
    }

    void setThumbColor(juce::Colour colour)
    {
        slider.setColour(juce::Slider::thumbColourId, colour);
    }

    juce::Slider& getSlider() { return slider; }
    juce::Label& getLabel() { return label; }

    int getMinimumRequiredWidth() const
    {
        auto font = label.getFont();
        const int labelWidth    = font.getStringWidth(label.getText());
        const int padding       = 20;
        const int minSliderBody = 60;

        return juce::jmax(minSliderBody, labelWidth + padding);
    }

private:
    juce::Label label;
    juce::Slider slider;
};
