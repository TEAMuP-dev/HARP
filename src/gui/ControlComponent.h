/**
 * @file ControlComponent.h
 * @brief Base class for control components with label and minimum size support.
 * @author gemini
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

/**
 * Abstract base class for control components that display a label.
 *
 * Each control owns its own sizing: the area laying them out asks for these
 * metrics rather than keeping a table of its own, so a control's appearance is
 * described in exactly one place.
 */
class ControlComponent : public Component
{
public:
    virtual ~ControlComponent() = default;

    /**
     * Returns the width this control should be given when there is room for it.
     * The layout falls back to getMinimumRequiredWidth() when space is tight.
     */
    virtual int getPreferredWidth() const = 0;

    /**
     * Returns the height this control needs in order to lay itself out.
     */
    virtual int getPreferredHeight() const = 0;

    /**
     * Returns the minimum width required to display this control properly.
     * This includes the label width plus any necessary padding.
     */
    virtual int getMinimumRequiredWidth() const = 0;

protected:
    /** Default padding around the label */
    static constexpr int defaultPadding = 20;

    /**
     * Calculates the width of a label's text using its current font.
     * @param label The label to measure
     * @return The width in pixels required to display the label text
     */
    int getLabelWidth(const Label& label) const
    {
        Font font = label.getFont();
        return font.getStringWidth(label.getText());
    }
};
