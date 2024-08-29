#include "LabelOverlayComponent.h"


LabelOverlayComponent::LabelOverlayComponent(String lab, String desc, float y, double t, double dur)
{
    label = lab;
    description = desc;
    offsetY = y;
    startTime = t;
    duration = dur;
}

LabelOverlayComponent::LabelOverlayComponent(const MidiNoteComponent& other)
{
    label = other.label;
    description = other.description;
    offsetY = other.offsetY;
    startTime = other.startTime;
    duration = other.duration;
}

void LabelOverlayComponent::paint(Graphics& g)
{
    // TODO
}
