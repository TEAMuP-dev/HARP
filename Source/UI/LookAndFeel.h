#include "juce_core/juce_core.h"
#include "juce_gui_basics/juce_gui_basics.h"

#pragma once

class HARPLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // using juce::LookAndFeel_V4::LookAndFeel_V4;
    HARPLookAndFeel()
    {


        // Set up the color palette
        setColour(juce::Slider::thumbColourId, highlightColor);
        setColour(juce::Slider::rotarySliderOutlineColourId, highlightColor);
        setColour(juce::Slider::trackColourId, platinum);
        setColour(juce::Slider::rotarySliderFillColourId, eerieBlack.brighter(0.5));

        setColour(juce::TextButton::buttonColourId, highlightColor);
        setColour(juce::TextButton::textColourOffId, white);

        setColour(juce::TextEditor::backgroundColourId, eerieBlack);
        setColour(juce::TextEditor::textColourId, white);
        setColour(juce::TextEditor::highlightColourId, highlightColor);

        setColour(juce::ComboBox::backgroundColourId, eerieBlack.brighter(0.5));
        setColour(juce::ComboBox::textColourId, white);

        setColour(juce::ResizableWindow::backgroundColourId, eerieBlack);

        setColour(juce::ScrollBar::thumbColourId, highlightColor);
    }

    juce::Colour highlightColor  = juce::Colour::fromString("0xff846BD3");
    juce::Colour textHeaderColor = juce::Colour::fromString("0xffCBFF8C");
    // Define the colors based on the new provided palette
    juce::Colour eerieBlack        = juce::Colour::fromString("0xff1c1c1c");
    juce::Colour celadon           = juce::Colour::fromString("0xff8fcb9b").darker();
    juce::Colour platinum          = juce::Colour::fromString("0xffeae6e5");
    juce::Colour savoyBlue         = juce::Colour::fromString("0xff5762d5");
    juce::Colour white             = juce::Colours::white;
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HARPLookAndFeel)
};
