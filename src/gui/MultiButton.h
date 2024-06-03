#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include <unordered_map>
#include <string>

class MultiButton : public juce::TextButton {
public:
    struct Mode {
        juce::String label;
        std::function<void()> callback;
        juce::Colour color;
    };

    MultiButton(const Mode& mode1, const Mode& mode2);

    MultiButton();

    void setMode(const juce::String& modeName);
    juce::String  getModeName();
    void addMode(const Mode& mode);
    // void toggleMode(const juce::String& newMode);

private:
    std::unordered_map<juce::String, Mode> modes;
    juce::String currentMode;
};
