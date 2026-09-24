/**
 * @file ToggleWithLabel.h
 * @brief Custom toggle button component with label.
 * @author gemini
 */

#pragma once

#include "ControlComponent.h"

using namespace juce;

class ToggleWithLabel : public ControlComponent
{
public:
    ToggleWithLabel(const String& labelText = {})
    {
        toggleButton.setButtonText(labelText);
        addAndMakeVisible(toggleButton);
    }

    void resized() override { toggleButton.setBounds(getLocalBounds()); }

    int getPreferredWidth() const override { return preferredToggleWidth; }

    int getPreferredHeight() const override { return preferredToggleHeight; }

    int getMinimumRequiredWidth() const override
    {
        const int textWidth = Font().getStringWidth(toggleButton.getButtonText());
        return jmax(minToggleWidth, textWidth + defaultPadding + checkboxWidthPadding);
    }

    ToggleButton& getToggleButton() { return toggleButton; }

    void setToggleState(bool state, NotificationType notification)
    {
        toggleButton.setToggleState(state, notification);
    }

    bool getToggleState() const { return toggleButton.getToggleState(); }

private:
    static constexpr int preferredToggleWidth = 112;
    static constexpr int preferredToggleHeight = 34;

    static constexpr int minToggleWidth = 60;
    static constexpr int checkboxWidthPadding = 30;

    ToggleButton toggleButton;
};
