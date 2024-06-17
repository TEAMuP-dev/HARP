#include "MultiButton.h"


MultiButton::MultiButton(const Mode& mode1, const Mode& mode2) {
    addMode(mode1);
    addMode(mode2);
    setMode(mode1.label);
}

MultiButton::MultiButton() {
    // Maybe set some default modes or leave for user to configure
}

void MultiButton::addMode(const Mode& mode) {
    // Check if the mode.label already exists
    if (modes.find(mode.label) != modes.end()) {
        // If it does, print a warning
        DBG("Mode with label " + mode.label + " already exists. Overwriting."); 
    }
    modes[mode.label] = mode;
}

void MultiButton::setMode(const juce::String& modeName) {
    if (modes.find(modeName) != modes.end() && currentMode != modeName) {
        currentMode = modeName;
        setButtonText(modes[currentMode].label);
        setColour(juce::TextButton::buttonColourId, modes[currentMode].color);
        onClick = modes[currentMode].callback; 
    }
}

void MultiButton::mouseEnter(const juce::MouseEvent& event) {
    if (onMouseEnter) {
        onMouseEnter();
    }
}

void MultiButton::mouseExit(const juce::MouseEvent& event) {
    if (onMouseExit) {
        onMouseExit();
    }
}

juce::String MultiButton::getModeName() {
    return currentMode;
}
