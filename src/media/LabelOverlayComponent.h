#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

using namespace juce;


class LabelOverlayComponent : public Label
{
public:

    LabelOverlayComponent(String lab, String desc, float y, double t, double dur);
    LabelOverlayComponent(const LabelOverlayComponent& other);

    void paint(Graphics& g);

    String getLabel() { return label; }
    String getDescription() { return description; }

    float getRelativeY() { return relativeY; }
    double getStartTime() { return startTime; }
    double getDuration() { return duration; }

private:

    String label;
    String description;

    float relativeY;
    double startTime;
    double duration;
};
