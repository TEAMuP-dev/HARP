#include "OutputLabelComponent.h"


LabelOverlayComponent::LabelOverlayComponent(double t, String lbl, float y)
{
    setTime(t);
    setLabel(lbl);
    setRelativeY(y);

    setDefaultAttributes();
}

LabelOverlayComponent::LabelOverlayComponent(double t, String lbl, float y, double dur) : LabelOverlayComponent(t, lbl, y)
{
    setDuration(dur);
}

LabelOverlayComponent::LabelOverlayComponent(double t, String lbl, float y, String dsc) : LabelOverlayComponent(t, lbl, y)
{
    setDescription(dsc);
}

LabelOverlayComponent::LabelOverlayComponent(double t, String lbl, float y, double dur, String dsc) : LabelOverlayComponent(t, lbl, y)
{
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

    setDefaultAttributes();
}

LabelOverlayComponent::~LabelOverlayComponent() {}

void LabelOverlayComponent::setDefaultAttributes()
{
    setText(getLabel(), dontSendNotification);
    setColour(Label::textColourId, Colours::white);
    setColour(Label::backgroundColourId, Colours::purple.withAlpha(0.5f));
    // TODO - set setMinimumHorizontalScale to something sensible

    setInterceptsMouseClicks(false, false);
}

float LabelOverlayComponent::amplitudeToRelativeY(float amplitude)
{
    return 1 - (amplitude + 1) / 2;
}

float LabelOverlayComponent::frequencyToRelativeY(float frequency)
{
    return 0.0f; // TODO
}

float LabelOverlayComponent::pitchToRelativeY(float pitch)
{
    return pitch / 128;
}
