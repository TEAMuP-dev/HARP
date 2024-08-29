#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

using namespace juce;


class LabelOverlayComponent : public Component
{
public:

    LabelOverlayComponent(String lab, String desc, float y, double t, double dur);
    LabelOverlayComponent(const LabelOverlayComponent& other);

    void paint(Graphics& g);

    String getLabel() { return label; }
    String getDescription() { return description; }

    float getOffsetY() { return offsetY; }
    double getStartTime() { return startTime; }
    double getNoteLength() { return duration; }

private:

    String label;
    String description;

    float offsetY;
    double startTime;
    double duration;
};
