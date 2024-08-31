#include "LabelOverlayComponent.h"


LabelOverlayComponent::LabelOverlayComponent(String lab, String desc, float y, double t, double dur)
{
    label = lab;
    description = desc;
    relativeY = y;
    startTime = t;
    duration = dur;
}

LabelOverlayComponent::LabelOverlayComponent(const LabelOverlayComponent& other)
{
    label = other.label;
    description = other.description;
    relativeY = other.relativeY;
    startTime = other.startTime;
    duration = other.duration;

    setText(label, dontSendNotification);
}

void LabelOverlayComponent::paint(Graphics& g)
{
    g.fillAll(Colours::purple);
}
