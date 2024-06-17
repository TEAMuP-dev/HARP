#include "HoverHandler.h"

HoverHandler::HoverHandler(juce::Component& target) : component(target) {}

void HoverHandler::attach() {
    component.addMouseListener(this, true);
}

void HoverHandler::detach() {
    component.removeMouseListener(this);
}

void HoverHandler::mouseEnter(const juce::MouseEvent& event) {
    if (onMouseEnter) {
        onMouseEnter();
    }
}

void HoverHandler::mouseExit(const juce::MouseEvent& event) {
    if (onMouseExit) {
        onMouseExit();
    }
}
