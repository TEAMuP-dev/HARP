#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include <functional>
#include "../external/fontaudio/src/FontAudio.h"
#include "../external/fontawesome/src/FontAwesome.h"
#include "CustomPathDialog.h"
#include "StatusComponent.h"

class HoverableLabel : public juce::Label
{
public:
    HoverableLabel() 
    {
        // Make sure the label reacts to mouse events
        originalTextColor = findColour(juce::Label::textColourId);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    // Override hitTest to allow hover only over text
    bool hitTest(int x, int y) override
    {
        // Get the text bounds
        auto textBounds = getTextBounds();

        // Check if the mouse coordinates are within the text bounds
        return textBounds.contains(x, y);
    }

    // Override mouse enter event
    void mouseEnter(const juce::MouseEvent& event) override
    {
        // Only change color if the mouse is over the text
        if (hitTest(event.x, event.y))
        {
            // Change text color to blue on hover
            setColour(juce::Label::textColourId, juce::Colours::coral);
            // setColour(juce::Label::backgroundColourId, juce::Colours::lightgrey); // Optional background color
            repaint(); // Request a repaint to reflect changes
            if (onHover) onHover(); // Call the hover callback if set
        }
        Label::mouseEnter(event); // Call base class
    }

    // Override mouse exit event
    void mouseExit(const juce::MouseEvent& event) override
    {
        // Reset label style back to the original color
        setColour(juce::Label::textColourId, originalTextColor); // Reset to original text color
        // setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack); // Reset background color
        if (onExit) onExit(); // Call the exit callback if set
        repaint(); // Request a repaint to reflect changes
        Label::mouseExit(event); // Call base class
    }

    // Override mouse down event (click event)
    void mouseDown(const juce::MouseEvent& event) override
    {
        if (hitTest(event.x, event.y) && onClick) // Check if clicking over the text
            onClick(); // Call the click callback if set
        Label::mouseDown(event); // Call base class
    }

    // Callback for hover event
    std::function<void()> onHover;
    std::function<void()> onExit;
    // Callback for click event
    std::function<void()> onClick;

private:

    juce::Rectangle<int> getTextBounds() const
    {
        auto font = getFont();
        auto textWidth = font.getStringWidth(getText());
        auto textHeight = font.getHeight();

        auto x_offset = (getBounds().getWidth() - textWidth) / 2;
        auto y_offset = (getBounds().getHeight() - textHeight) / 2;
        
        // Calculate the bounds considering the label's position
        return juce::Rectangle<int>(getX() + x_offset, getY() + y_offset, textWidth, textHeight);
    }

    juce::Colour originalTextColor;
};

class ModelAuthorLabel : public juce::Component
{
public:
    ModelAuthorLabel() //juce::Label& modelLabel, juce::Label& authorLabel, const juce::URL& url
        // :  url(url) //modelLabel(modelLabel), authorLabel(authorLabel),
    {
        modelLabel.onHover = [this] { 
            instructionBox->setStatusMessage("Click to view the model's page");
        };

        modelLabel.onExit = [this] {
            instructionBox->clearStatusMessage();
        };

        modelLabel.onClick = [this] {
            url.launchInDefaultBrowser();
        };

        addAndMakeVisible(modelLabel);
        addAndMakeVisible(authorLabel);
        
    }

    void resized() override
    {
        auto area = getLocalBounds();
        modelLabel.setFont(juce::Font(22.0f, juce::Font::bold));
        auto nameWidth =
            modelLabel.getFont().getStringWidthFloat(modelLabel.getText()) + 10; // Add some padding
        modelLabel.setBounds(area.removeFromLeft(static_cast<int>(nameWidth)));
        authorLabel.setBounds(area);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void setModelText(const juce::String& name)
    {
        modelLabel.setText(name, juce::dontSendNotification);
    }
    void setAuthorText(const juce::String& name)
    {
        authorLabel.setText(name, juce::dontSendNotification);
    }
    void setURL(const juce::URL& newURL)
    {
        url = newURL;
    }

private:
    HoverableLabel modelLabel;
    juce::Label authorLabel;
    juce::URL url;
    juce::SharedResourcePointer<InstructionBox> instructionBox;
};