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
    modes[mode.label] = mode;
}

void MultiButton::setMode(const juce::String& modeName) {
    if (modes.find(modeName) != modes.end()) {
        // currentMode = modeName;
        setButtonText(modes[modeName].label);
        setColour(juce::TextButton::buttonColourId, modes[modeName].color);
        onClick = modes[modeName].callback; 
    }
}

// void MultiButton::setModeProperty(const juce::String& modeName, const Mode& mode) {
//     if (modes.find(modeName) != modes.end()) {
//         modes[modeName] = mode;
//     }
// }
