/**
 * @file HoverableLabel.h
 * @brief A custom label that is clickable and changes color when hovering over the text
 * @author xribene
 */
#pragma once
#include "juce_gui_basics/juce_gui_basics.h"
#include <functional>

class HoverableLabel : public juce::Label
{
public:
    HoverableLabel()
    {
        // Save the original text color. We need it to reset the color on mouse exit
        originalTextColor = findColour(juce::Label::textColourId);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    // From JUCE doc
    // hitTest() Tests whether a given point is inside the component.
    // Overriding this method allows you to create components which only
    // intercept mouse-clicks within a user-defined area
    // We override it so that we can check if the mouse is over the text
    bool hitTest(int x, int y) override
    {
        // Get the text bounds
        auto textBounds = getTextBounds();
        // Check if the mouse coordinates are within the text bounds
        return textBounds.contains(x, y);
    }

    void mouseEnter(const juce::MouseEvent& event) override
    {
        // Only change color if the mouse is over the text
        if (hitTest(event.x, event.y))
        {
            // Change text color to hover color
            setColour(juce::Label::textColourId, hoverColor);
            // setColour(juce::Label::backgroundColourId, juce::Colours::lightgrey);
            repaint();
            // Call the hover callback if set
            if (onHover)
                onHover();
        }
        // Call base class
        Label::mouseEnter(event);
    }

    void mouseExit(const juce::MouseEvent& event) override
    {
        // Reset label style back to the original color
        setColour(juce::Label::textColourId, originalTextColor);
        // setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack); // Reset background color
        // Call the exit callback if set
        if (onExit)
            onExit(); 
        repaint();
        // Call base class
        Label::mouseExit(event); 
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        // Check if clicking over the text
        // and call the click callback if set
        if (hitTest(event.x, event.y) && onClick) 
            onClick();
        Label::mouseDown(event);
    }

    void setHoverColour(juce::Colour color) { hoverColor = color; }

    // Callback for hover event
    std::function<void()> onHover;
    std::function<void()> onExit;
    // Callback for click event
    std::function<void()> onClick;
    // Color for hoverOn event
    juce::Colour hoverColor = juce::Colours::blue;

private:
    juce::Rectangle<int> getTextBounds() const
    {
        auto font = getFont();
        auto textWidth = font.getStringWidth(getText());
        auto textHeight = font.getHeight();

        auto x_offset = (getBounds().getWidth() - textWidth) / 2;
        auto y_offset = (getBounds().getHeight() - textHeight) / 2;

        return juce::Rectangle<int>(getX() + x_offset, getY() + y_offset, textWidth, textHeight);
    }

    juce::Colour originalTextColor;
};