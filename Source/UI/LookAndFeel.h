#include "juce_core/juce_core.h"
#include "juce_gui_basics/juce_gui_basics.h"

class HARPLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // using juce::LookAndFeel_V4::LookAndFeel_V4;
    HARPLookAndFeel()
    {
        // Define the colors based on the new provided palette
        auto eerieBlack        = juce::Colour::fromString("0xff1c1c1c");
        auto mountbattenPink   = juce::Colour::fromString("0xffa4778b");
        auto celadon           = juce::Colour::fromString("0xff8fcb9b").darker();
        auto platinum          = juce::Colour::fromString("0xffeae6e5");
        auto savoyBlue         = juce::Colour::fromString("0xff5762d5");
        auto white             = juce::Colours::white;

        // Set up the color palette
        setColour(juce::Slider::thumbColourId, mountbattenPink);
        setColour(juce::Slider::rotarySliderOutlineColourId, mountbattenPink);
        setColour(juce::Slider::trackColourId, platinum);
        setColour(juce::Slider::rotarySliderFillColourId, eerieBlack.brighter(0.5));

        setColour(juce::TextButton::buttonColourId, mountbattenPink);
        setColour(juce::TextButton::textColourOffId, white);

        setColour(juce::TextEditor::backgroundColourId, eerieBlack);
        setColour(juce::TextEditor::textColourId, white);
        setColour(juce::TextEditor::highlightColourId, mountbattenPink);

        setColour(juce::ComboBox::backgroundColourId, eerieBlack.brighter(0.5));
        setColour(juce::ComboBox::textColourId, white);

        setColour(juce::ResizableWindow::backgroundColourId, eerieBlack);

        setColour(juce::ScrollBar::thumbColourId, mountbattenPink);
    }

private:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HARPLookAndFeel)
};
