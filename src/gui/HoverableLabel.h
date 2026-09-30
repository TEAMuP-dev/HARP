/**
 * @file HoverableLabel.h
 * @brief Custom clickable label that changes color when hovering over the text.
 * @author xribene
 */

#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

class HoverableLabel : public Label
{
public:
    HoverableLabel()
    {
        // Save original text color for resets
        originalTextColor = findColour(Label::textColourId);

        setHoverable(true);
    }

    bool isHoverable() { return hoverable; }

    void setHoverable(bool h)
    {
        hoverable = h;

        if (hoverable)
        {
            setMouseCursor(MouseCursor::PointingHandCursor);
        }
        else
        {
            setMouseCursor(MouseCursor::NormalCursor);
        }
    }

    /**
     * Overriden component function to test if mouse is over text.
     */
    bool hitTest(int x, int y) override
    {
        Rectangle<int> textBounds = getTextBounds();

        // Check if mouse coordinates are within bounds
        return textBounds.contains(x, y);
    }

    void mouseEnter(const MouseEvent& event) override
    {
        // Change color if mouse is over text
        if (hitTest(event.x, event.y))
        {
            // Change text color to hover color
            setColour(Label::textColourId, hoverColor);

            if (onHover && hoverable)
            {
                // Call hover callback if set
                onHover();
            }
        }

        Label::mouseEnter(event);
    }

    void mouseExit(const MouseEvent& event) override
    {
        // Reset label back to original color
        setColour(Label::textColourId, originalTextColor);

        if (onExit && hoverable)
        {
            // Call exit callback if set
            onExit();
        }

        Label::mouseExit(event);
    }

    void mouseDown(const MouseEvent& event) override
    {
        if (hitTest(event.x, event.y) && onClick && hoverable)
        {
            // Call click callback if set
            onClick();
        }

        Label::mouseDown(event);
    }

    void setHoverColor(Colour color) { hoverColor = color; }

    // Callbacks for mouse events
    std::function<void()> onHover;
    std::function<void()> onExit;
    std::function<void()> onClick;

private:
    /* Where the text is drawn, in this label's own coordinates (which are what hitTest is
       given), allowing for its border and justification */
    Rectangle<int> getTextBounds() const
    {
        const Font f = getFont();
        const Rectangle<int> textSize(GlyphArrangement::getStringWidthInt(f, getText()),
                                      roundToInt(f.getHeight()));

        return getJustificationType().appliedToRectangle(
            textSize, getBorderSize().subtractedFrom(getLocalBounds()));
    }

    bool hoverable;

    Colour originalTextColor;
    Colour hoverColor = Colours::blue;
};
