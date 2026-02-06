/**
 * @file Interface.h
 * @brief Simple helper functions for interface.
 * @author hugofloresgarcia
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

inline Colour getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour uiColour,
                                     Colour fallback = Colour(0xff4d4d4d)) noexcept
{
    if (auto* v4 = dynamic_cast<LookAndFeel_V4*>(&LookAndFeel::getDefaultLookAndFeel()))
    {
        return v4->getCurrentColourScheme().getUIColour(uiColour);
    }

    return fallback;
}
