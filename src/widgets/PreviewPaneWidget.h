/**
 * @file PreviewPaneWidget.h
 * @brief Component that allows for the previewing of MediaComponents.
 * @author NatalieElizabeth
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

class PreviewPaneWidget : public Component
{
public:
    PreviewPaneWidget() {}

    void paint(Graphics& g) override
    {
        g.fillAll(Colour(0x1a, 0x1a, 0x2e));
        g.setColour(Colours::white);
        g.drawText("Preview Pane (TODO)", getLocalBounds(), Justification::centred);
    }

    void resized() override {}
};