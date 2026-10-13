/**
 * @file ResizeEdge.h
 * @brief Strip along the edge of a panel that is dragged to resize it.
 * @author cwitkowitz, NatalieElizabeth
 */

#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Palette.h"

using namespace juce;

/**
 * Strip along the left or top edge of a panel that is dragged to resize it. It lights up in
 * the accent colour while hovered, as a card's outline does.
 */
class ResizeEdge : public Component
{
public:
    enum class Side
    {
        Left,
        Top
    };

    explicit ResizeEdge(Side edgeSide) : side(edgeSide)
    {
        setMouseCursor(side == Side::Left ? MouseCursor::LeftRightResizeCursor
                                          : MouseCursor::UpDownResizeCursor);
    }

    // Called once when a drag begins
    std::function<void()> onDragStart;
    // Called throughout a drag with how far right (or down) the mouse is from where it began
    std::function<void(int)> onDrag;

    void mouseDown(const MouseEvent&) override
    {
        if (onDragStart)
        {
            onDragStart();
        }
    }

    void mouseDrag(const MouseEvent& e) override
    {
        if (onDrag)
        {
            onDrag(side == Side::Left ? e.getScreenX() - e.getMouseDownScreenX()
                                      : e.getScreenY() - e.getMouseDownScreenY());
        }
    }

    void mouseUp(const MouseEvent&) override { repaint(); }
    void mouseEnter(const MouseEvent&) override { repaint(); }
    void mouseExit(const MouseEvent&) override { repaint(); }

    void paint(Graphics& g) override
    {
        const bool isActive = isMouseOverOrDragging();

        // A left edge also divides its panel from the one beside it, so it shows while idle
        if (side == Side::Left)
        {
            g.setColour(isActive ? ModelStyle::accent.withAlpha(0.6f) : ModelStyle::outline);
            g.fillRect(getLocalBounds().removeFromLeft(isActive ? 2 : 1));
        }
        else if (isActive)
        {
            g.setColour(ModelStyle::accent.withAlpha(0.6f));
            g.fillRect(getLocalBounds().removeFromTop(2));
        }
    }

private:
    const Side side;
};
