/**
 * @file TutorialTargets.h
 * @brief Parts of the interface the tutorial can point at, and how it finds them.
 * @author cwitkowitz, saumya-pailwan
 *
 * A component is marked for the tutorial by giving it one of these IDs (e.g.,
 * processButton.setComponentID(TutorialTargets::processButton)). The tutorial then finds
 * it in the window by that ID, so nothing else about it needs to be exposed.
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

namespace TutorialTargets
{
inline constexpr const char* tabBar = "tutorial.tabBar";
inline constexpr const char* modelTabs = "tutorial.modelTabs";
inline constexpr const char* modelSearch = "tutorial.modelSearch";
inline constexpr const char* modelList = "tutorial.modelList";
inline constexpr const char* controls = "tutorial.controls";
inline constexpr const char* inputTracks = "tutorial.inputTracks";
inline constexpr const char* outputTracks = "tutorial.outputTracks";
inline constexpr const char* trackFolderButton = "tutorial.trackFolderButton";
inline constexpr const char* trackPlayButton = "tutorial.trackPlayButton";
inline constexpr const char* processButton = "tutorial.processButton";
inline constexpr const char* statusArea = "tutorial.statusArea";
inline constexpr const char* clipboard = "tutorial.clipboard";
inline constexpr const char* clipboardControls = "tutorial.clipboardControls";

/**
 * Something for the tutorial to highlight: the first showing component with an ID,
 * optionally only within another (e.g., the folder button of the first input track), or
 * failing that, another component (e.g., the tab bar when the Home tab is not showing).
 */
struct Target
{
    String id;
    String within = {};
    String otherwise = {};
};

// The first showing component with an ID, searching depth-first in child order
inline Component* findShowing(Component& root, const String& id)
{
    for (auto* child : root.getChildren())
    {
        if (! child->isVisible())
            continue;

        if (child->getComponentID() == id && child->isShowing())
            return child;

        if (auto* found = findShowing(*child, id))
            return found;
    }

    return nullptr;
}

/* Where a component is in root's coordinates, clipped to the part that is actually on screen
   (e.g., within a scrolled viewport), or empty if none of it is */
inline Rectangle<int> getVisibleArea(Component& root, Component& component)
{
    Rectangle<int> area = component.getLocalBounds();

    for (auto* c = &component; c != &root; c = c->getParentComponent())
    {
        auto* parent = c->getParentComponent();

        if (parent == nullptr)
            return {};

        area = (area + c->getPosition()).getIntersection(parent->getLocalBounds());
    }

    return area;
}

inline Rectangle<int> locate(Component& root, const String& id, const String& within)
{
    Component* scope = &root;

    if (within.isNotEmpty())
        scope = findShowing(root, within);

    auto* component = scope != nullptr ? findShowing(*scope, id) : nullptr;

    return component != nullptr ? getVisibleArea(root, *component) : Rectangle<int>();
}

// Where a target is in root's coordinates, or empty if it is not on screen
inline Rectangle<int> locate(Component& root, const Target& target)
{
    const auto area = locate(root, target.id, target.within);

    if (area.isEmpty() && target.otherwise.isNotEmpty())
        return locate(root, target.otherwise, {});

    return area;
}
} // namespace TutorialTargets
