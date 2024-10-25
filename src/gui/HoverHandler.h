#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include <functional>

class HoverHandler : public juce::MouseListener {
public:
    HoverHandler(juce::Component& target);

    void attach();
    void detach();

    std::function<void()> onMouseEnter;
    std::function<void()> onMouseMove;
    std::function<void()> onMouseExit;

private:
    juce::Component& component;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
};
