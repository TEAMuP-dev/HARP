/**
 * @file HoverHandler.h
 * @brief Attach custom event handling functionality to an arbitrary component.
 * @author xribene
 */

#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

class HoverHandler : public MouseListener
{
public:
    HoverHandler(Component& target) : component(target) {}

    void attach() { component.addMouseListener(this, true); }

    void detach() { component.removeMouseListener(this); }

    std::function<void()> onMouseEnter;
    std::function<void()> onMouseMove;
    std::function<void()> onMouseExit;

private:
    void mouseEnter(const MouseEvent& /*event*/) override
    {
        if (onMouseEnter)
        {
            onMouseEnter();
        }
    }

    void mouseMove(const MouseEvent& /*event*/) override
    {
        if (onMouseMove)
        {
            onMouseMove();
        }
    }

    void mouseExit(const MouseEvent& /*event*/) override
    {
        if (onMouseExit)
        {
            onMouseExit();
        }
    }

    Component& component;
};
