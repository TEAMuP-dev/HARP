/*
 * @file MediaClipboard.h
 * @brief The component that manages open files within HARP.
 * @author cwitkowitz
 */

#pragma once
#include "juce_gui_basics/juce_gui_basics.h"

class MediaClipboardWidget : public juce::Component
{
public:
    MediaClipboardWidget() {}

    void resized() override
    {
        auto area = getLocalBounds();

        juce::FlexBox mainBox;
        mainBox.flexDirection =
            juce::FlexBox::Direction::column; // Set the main flex direction to column

        juce::FlexItem::Margin margin(2);

        // Perform Layout
        mainBox.performLayout(area);
    }

private:
};
