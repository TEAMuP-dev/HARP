#include "juce_core/juce_core.h"
#include "juce_gui_basics/juce_gui_basics.h"

class HARPLookAndFeel : public juce::LookAndFeel_V4
{
public:
    using juce::LookAndFeel_V4::LookAndFeel_V4;
    // HARPLookAndFeel()
    // {
    //     // Define your colors based on the provided palette
    //     hunterGreen       = juce::Colour::fromString("#3e5641");
    //     chestnut          = juce::Colour::fromString("#a24936");
    //     flame             = juce::Colour::fromString("#d36135");
    //     jet               = juce::Colour::fromString("#282b28");
    //     cambridgeBlue     = juce::Colour::fromString("#83bca9");
    // }

    // void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
    //                            bool isMouseOverButton, bool isButtonDown) override
    // {
    //     auto buttonArea = button.getLocalBounds();
    //     auto edge = buttonArea.withSizeKeepingCentre (buttonArea.getWidth() - 2, buttonArea.getHeight() - 2);

    //     g.setColour (isButtonDown ? flame : isMouseOverButton ? chestnut : hunterGreen);
    //     g.fillRoundedRectangle (edge.toFloat(), 6.0f);
    // }

    //     void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
    //                        float sliderPos, float minSliderPos, float maxSliderPos,
    //                        const juce::Slider::SliderStyle style, juce::Slider& slider) override
    // {
    //     g.fillAll (slider.findColour (juce::Slider::backgroundColourId));

    //     if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical)
    //     {
    //         g.setColour (flame);
    //         g.fillRect (slider.isHorizontal() ? juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y), sliderPos - static_cast<float> (x), static_cast<float> (height))
    //                                            : juce::Rectangle<float> (static_cast<float> (x), sliderPos, static_cast<float> (width), static_cast<float> (y + height) - sliderPos));
    //     }
    //     else
    //     {
    //         g.setColour (chestnut);
    //         g.drawRoundedRectangle (juce::Rectangle<float> (x, y, width, height).reduced (0.5f), 5.0f, 1.0f);
    //     }
    // }

    // void drawLabel (juce::Graphics& g, juce::Label& label) override
    // {
    //     g.fillAll (label.findColour (juce::Label::backgroundColourId));

    //     if (! label.isBeingEdited())
    //     {
    //         auto alpha = label.isEnabled() ? 1.0f : 0.5f;
    //         g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
    //         g.drawFittedText (label.getText(), label.getLocalBounds(),
    //                           label.getJustificationType(),
    //                           juce::jmax (1, (int) (label.getHeight() / label.getFont().getHeight())),
    //                           label.getMinimumHorizontalScale());

    //         g.setColour (label.findColour (juce::Label::outlineColourId).withMultipliedAlpha (alpha));
    //     }
    //     else if (label.isEnabled())
    //     {
    //         g.setColour (label.findColour (juce::Label::outlineColourId));
    //     }

    //     g.drawRect (label.getLocalBounds());
    // }

    // void drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& textEditor) override
    // {
    //     if (textEditor.isEnabled())
    //     {
    //         if (textEditor.hasKeyboardFocus (false) && ! textEditor.isReadOnly())
    //         {
    //             auto border = textEditor.findColour (juce::TextEditor::focusedOutlineColourId);
    //             g.setColour (border);
    //         }
    //         else
    //         {
    //             g.setColour (jet);
    //         }

    //         g.drawRect (0, 0, width, height);
    //     }
    // }


private:
    juce::Colour hunterGreen, chestnut, flame, jet, cambridgeBlue;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HARPLookAndFeel)
};