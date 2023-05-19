class ToolbarSliderStyle : public juce::LookAndFeel_V4
{
public:
    ToolbarSliderStyle()
    {
        setColour(juce::Slider::backgroundColourId, juce::Colours::black);
        setColour(juce::Slider::thumbColourId, juce::Colours::white);
    }
    Font getTextButtonFont(TextButton& button, int buttonHeight) override
    {
        return Font(2.0f, Font::plain); 
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          const float rotaryStartAngle,
                          const float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        auto radius = (float) juce::jmin(width / 2, height / 2) - 2.0f;
        auto centreX = (float) x + (float) width  * 0.5f;
        auto centreY = (float) y + (float) height * 0.5f;
        auto rx = centreX - radius;
        auto ry = centreY - radius;
        auto rw = radius * 2.0f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // fill
        g.setColour(juce::Colours::black);
        g.fillEllipse(rx, ry, rw, rw);

        // outline
        g.setColour(juce::Colours::black);
        g.drawEllipse(rx, ry, rw, rw, 1.0f);

        juce::Path p;
        auto pointerLength = radius * 0.33f;
        auto pointerThickness = 2.0f;
        p.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));

        // pointer
        g.setColour(juce::Colours::white);
        g.fillPath(p);
    }

    void drawLabel(Graphics& g, Label& label) override
    {
    g.fillAll (label.findColour (Label::backgroundColourId));

    if (! label.isBeingEdited())
    {
        const float alpha = label.isEnabled() ? 1.0f : 0.5f;
        const Font font (getLabelFont (label));

        g.setColour (label.findColour (Label::textColourId).withMultipliedAlpha (alpha));
        g.setFont (Font(16.0, Font::plain));

        Rectangle<int> textArea (label.getBorderSize().subtractedFrom (label.getLocalBounds()));

        g.drawFittedText (label.getText(), textArea, label.getJustificationType(),
                          jmax (1, (int) (textArea.getHeight() / font.getHeight())),
                          label.getMinimumHorizontalScale());

        g.setColour (label.findColour (Label::outlineColourId).withMultipliedAlpha (alpha));
    }
    else if (label.isEnabled())
    {
        g.setColour (label.findColour (Label::outlineColourId));
    }

    }

};


class ButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ButtonLookAndFeel()
    {
        setColour (TextButton::buttonColourId, juce::Colours::black);
        setColour (TextButton::buttonOnColourId, juce::Colours::black.brighter());
        setColour (TextButton::textColourOffId, juce::Colours::white);
        setColour (TextButton::textColourOnId, juce::Colours::white);
    }

    Font getTextButtonFont(TextButton& button, int buttonHeight) override
    {
        return Font(14.0f, Font::plain); 
    }

    void drawButtonBackground (Graphics& g, Button& button, const Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto buttonArea = button.getLocalBounds();
        auto edge = 4;

        buttonArea.removeFromTop (edge);
        buttonArea.removeFromBottom (edge);
        buttonArea.removeFromLeft (edge);
        buttonArea.removeFromRight (edge);

        g.setColour (shouldDrawButtonAsHighlighted ? findColour (TextButton::buttonOnColourId)
                                                    : findColour (TextButton::buttonColourId));
        g.fillRoundedRectangle (buttonArea.toFloat(), edge);
    }

    void drawButtonText (Graphics& g, TextButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        g.setColour (button.findColour (button.getToggleState() ? TextButton::textColourOnId
                                                                : TextButton::textColourOffId)
                    .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));

        // set text colour to white
        g.setColour (Colours::white);

        const int yIndent = jmin (4, button.proportionOfHeight (0.1f)); // Reduced indent proportion
        const int cornerSize = jmin (button.getHeight(), button.getWidth()) / 2;

        const int fontHeight = roundToInt (cornerSize * 0.6f);
        const int leftIndent  = jmin (fontHeight, 2 + cornerSize / (button.isConnectedOnLeft() ? 4 : 2));
        const int rightIndent = jmin (fontHeight, 2 + cornerSize / (button.isConnectedOnRight() ? 4 : 2));
        const int textWidth = button.getWidth() - leftIndent - rightIndent;

        if (textWidth > 0)
        {
            g.setFont (Font(16.0, Font::plain));
            g.drawFittedText(button.getButtonText(),
                    leftIndent, yIndent, // top-left position with new indent
                    button.getWidth() - 2*leftIndent, button.getHeight() - 2*yIndent, // new width and height
                    Justification::centred, // centre the text both horizontally and vertically
                    2); // maximum number of lines the text can use
        }
    }


};

class ComboBoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ComboBoxLookAndFeel()
    {
        setColour(ComboBox::backgroundColourId, juce::Colours::black);
        setColour(ComboBox::textColourId, juce::Colours::white);
        setColour(ComboBox::arrowColourId, juce::Colours::white);
        setColour(ComboBox::outlineColourId, juce::Colours::white);

        setColour(PopupMenu::backgroundColourId, juce::Colours::black);
        setColour(PopupMenu::textColourId, juce::Colours::white);
        setColour(PopupMenu::highlightedTextColourId, juce::Colours::white);
    }
    Font getTextButtonFont(TextButton& button, int buttonHeight) override
    {
        return Font(14.0f, Font::plain); 
    }
void drawComboBox (Graphics& g, int width, int height, bool /*isButtonDown*/,
                   int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                   ComboBox& box) override
{
    auto cornerSize = box.findParentComponentOfClass<ChoicePropertyComponent>() != nullptr ? 0.0f : 3.0f;
    Rectangle<int> boxBounds (0, 0, width, height);

    g.setColour (box.findColour (ComboBox::backgroundColourId));
    g.fillRoundedRectangle (boxBounds.toFloat(), cornerSize);

    g.setColour (box.findColour (ComboBox::outlineColourId));
    g.drawRoundedRectangle (boxBounds.toFloat().reduced (0.5f, 0.5f), cornerSize, 1.0f);

    auto arrowX = static_cast<float> (width - height * 0.5f);
    auto arrowH = static_cast<float> (height * 0.35f * 0.5f);  // Scale down the height by 50%

    Path path;
    path.startNewSubPath (arrowX, height * 0.3f + arrowH * 0.5f); // Center the arrow
    path.lineTo (arrowX + arrowH * 0.6f, height * 0.7f - arrowH * 0.5f); // Adjust the arrow point
    path.lineTo (arrowX - arrowH * 0.6f, height * 0.7f - arrowH * 0.5f); // Adjust the arrow point
    path.closeSubPath();
    path.applyTransform(AffineTransform::rotation(-M_PI, arrowX, height * 0.5f)); // Rotate the arrow to point downwards

    g.setColour (box.findColour (ComboBox::arrowColourId).withAlpha ((box.isEnabled() ? 1.0f : 0.3f)));
    g.fillPath (path); // Fill the triangle instead of stroke
}

void drawPopupMenuItem (Graphics& g, const Rectangle<int>& area,
                        const bool isSeparator, const bool isActive,
                        const bool isHighlighted, const bool isTicked,
                        const bool hasSubMenu, const String& text,
                        const String& shortcutKeyText,
                        const Drawable* icon, const Colour* const textColourToUse) override
{
    if (isSeparator)
    {
        auto r  = area.reduced (5, 0);
        r.removeFromTop (roundToInt ((r.getHeight() * 0.5f) - 0.5f));

        g.setColour (findColour (PopupMenu::textColourId).withAlpha (0.3f));
        g.fillRect (r.removeFromTop (1));
    }
    else
    {
        auto textColour = (textColourToUse == nullptr ? findColour (PopupMenu::textColourId)
                          : *textColourToUse);

        auto r  = area.reduced (1);

        if (isHighlighted)
        {
            g.setColour (findColour (PopupMenu::highlightedBackgroundColourId));
            g.fillRect (r);

            g.setColour (findColour (PopupMenu::highlightedTextColourId));
        }
        else
        {
            g.setColour (textColour.withMultipliedAlpha (isActive ? 1.0f : 0.5f));
        }

        r.reduce (jmin (5, area.getWidth() / 20), 0);

        auto font = getPopupMenuFont();

        const auto maxFontHeight = r.getHeight() / 1.3f;

        if (font.getHeight() > maxFontHeight)
            font.setHeight (maxFontHeight);

        g.setFont (Font(16.0, Font::plain));

        auto iconArea = r.removeFromLeft (roundToInt (maxFontHeight)).toFloat();

        if (icon != nullptr)
        {
            icon->drawWithin (g, iconArea, RectanglePlacement::centred | RectanglePlacement::onlyReduceInSize, 1.0f);
        }
        else if (isTicked)
        {
            g.drawText ("*", iconArea, Justification::centred);
        }

        if (hasSubMenu)
        {
            auto arrowH = 0.6f * getPopupMenuFont().getAscent();

            auto x = static_cast<float> (r.removeFromRight ((int) arrowH).getX());
            auto halfH = static_cast<float> (r.getCentreY());

            Path path;
            path.startNewSubPath (x, halfH - arrowH * 0.5f);
            path.lineTo (x + arrowH * 0.6f, halfH);
            path.lineTo (x, halfH + arrowH * 0.5f);

            g.strokePath (path, PathStrokeType (2.0f));
        }

        r.removeFromRight (3);
        g.drawFittedText (text, r, Justification::centredLeft, 1);

        if (shortcutKeyText.isNotEmpty())
        {
            auto f = font;
            f.setHeight (f.getHeight() * 0.75f);
            f.setHorizontalScale (0.95f);
            g.setFont (Font(16.0, Font::plain));
            g.drawText (shortcutKeyText, r, Justification::centredRight, true);
        }
    }
}



};
