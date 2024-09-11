#include "LabelOverlayComponent.h"


LabelOverlayComponent::LabelOverlayComponent(double t, String lbl, float y)
{
    setTime(t);
    setLabel(lbl);
    setRelativeY(y);
}

LabelOverlayComponent::LabelOverlayComponent(double t, String lbl, float y, double dur)
{
    LabelOverlayComponent(t, lbl, y);
    setDuration(dur);
}

LabelOverlayComponent::LabelOverlayComponent(double t, String lbl, float y, String dsc)
{
    LabelOverlayComponent(t, lbl, y);
    setDescription(dsc);
}

LabelOverlayComponent::LabelOverlayComponent(double t, String lbl, float y, double dur, String dsc)
{
    LabelOverlayComponent(t, lbl, y);
    setDuration(dur);
    setDescription(dsc);
}

LabelOverlayComponent::LabelOverlayComponent(const LabelOverlayComponent& other)
{
    setTime(other.getTime());
    setLabel(other.getLabel());
    setRelativeY(other.getRelativeY());
    setDuration(other.getDuration());
    setDescription(other.getDescription());
}

void LabelOverlayComponent::paint(Graphics& g)
{
    g.fillAll(Colours::purple);
}

float LabelOverlayComponent::amplitudeToRelativeY(float amplitude)
{
    return (amplitude + 1) / 2;
}

float LabelOverlayComponent::frequencyToRelativeY(float frequency)
{
    return 0.0f; // TODO
}

float LabelOverlayComponent::pitchToRelativeY(float pitch)
{
    return pitch / 128;
}
