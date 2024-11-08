#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include <string>
#include <unordered_map>

class MultiButton : public juce::TextButton
{
public:
    struct Mode
    {
        juce::String label;
        std::function<void()> callback;
        juce::Colour color;
    };

    std::function<void()> onMouseEnter;
    std::function<void()> onMouseExit;

    MultiButton(const Mode& mode1, const Mode& mode2);

    MultiButton();

    void setMode(const juce::String& modeName);
    juce::String getModeName();
    void addMode(const Mode& mode);
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

private:
    std::unordered_map<juce::String, Mode> modes;
    juce::String currentMode;
};
