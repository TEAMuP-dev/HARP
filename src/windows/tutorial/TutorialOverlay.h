/**
 * @file TutorialOverlay.h
 * @brief Dims the window around whatever the current tutorial step points at.
 * @author cwitkowitz, saumya-pailwan
 */

#pragma once

#include <functional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "TutorialTargets.h"

using namespace juce;

/**
 * Laid over an area of the window (without taking any of its mouse clicks), dimming all of it
 * except the targets of the current step, which are outlined.
 *
 * The targets are located again a few times a second, so that the highlights follow them as
 * the window is resized, a tab is scrolled or switched, or a model finishes loading, without
 * any of those having to notify the tutorial.
 */
class TutorialOverlay : public Component, private Timer
{
public:
    explicit TutorialOverlay(Component& areaToCover) : area(areaToCover)
    {
        setInterceptsMouseClicks(false, false);
        area.addAndMakeVisible(this);

        update();
        startTimerHz(refreshRateHz);
    }

    ~TutorialOverlay() override { area.removeChildComponent(this); }

    /* Called when a popup opens in the area, and again when it closes. The tutorial window
       floats over the middle of the area, which is where a popup opens. */
    std::function<void(bool)> onPopupShowingChanged;

    // What to highlight, or nothing to dim the whole area
    void setTargets(std::vector<TutorialTargets::Target> newTargets)
    {
        targets = std::move(newTargets);

        update();
    }

    void paint(Graphics& g) override
    {
        g.setColour(Colours::black.withAlpha(0.6f));

        {
            Graphics::ScopedSaveState state(g);

            for (const auto& highlight : highlights)
                g.excludeClipRegion(highlight);

            g.fillAll();
        }

        g.setColour(Colours::white);

        for (const auto& highlight : highlights)
            g.drawRoundedRectangle(highlight.toFloat().reduced(1.0f), 5.0f, 2.0f);
    }

private:
    void timerCallback() override { update(); }

    void update()
    {
        if (getBounds() != area.getLocalBounds())
            setBounds(area.getLocalBounds());

        // Other overlays (e.g., for dragging tracks) may have been brought forward since
        toFront(false);

        std::vector<Rectangle<int>> located;

        for (const auto& target : targets)
        {
            if (const auto bounds = TutorialTargets::locate(area, target); ! bounds.isEmpty())
                located.push_back(bounds.expanded(2).getIntersection(getLocalBounds()));
        }

        // A popup has to stay readable, since this overlay sits in front of it as well
        const auto popupBounds = TutorialTargets::locate(area, TutorialTargets::popup, {});

        if (! popupBounds.isEmpty())
        {
            located.push_back(popupBounds.expanded(2).getIntersection(getLocalBounds()));
        }

        if (const bool isShowing = ! popupBounds.isEmpty(); isShowing != isPopupShowing)
        {
            isPopupShowing = isShowing;

            if (onPopupShowingChanged)
                onPopupShowingChanged(isShowing);
        }

        if (located != highlights)
        {
            highlights = std::move(located);
            repaint();
        }
    }

    static constexpr int refreshRateHz = 8;

    Component& area;

    std::vector<TutorialTargets::Target> targets;
    std::vector<Rectangle<int>> highlights;

    bool isPopupShowing = false;
};
