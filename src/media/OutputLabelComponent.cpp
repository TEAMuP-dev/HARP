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

LabelOverlayComponent::LabelOverlayComponent(double t, String lbl, float y, double dur, String dsc, Colour clr) : LabelOverlayComponent(t, lbl, y)
{
    setDuration(dur);
    setDescription(dsc);
    setColor(clr);
    setColour(Label::backgroundColourId, clr);
    repaint();
}

LabelOverlayComponent::LabelOverlayComponent(const LabelOverlayComponent& other)
{
    setTime(other.getTime());
    setLabel(other.getLabel());
    setRelativeY(other.getRelativeY());
    setDuration(other.getDuration());
    setDescription(other.getDescription());
    setDefaultAttributes();
    setColor(other.getColor());
    setColour(Label::backgroundColourId, other.getColor());

    
}

LabelOverlayComponent::~LabelOverlayComponent() {}

void LabelOverlayComponent::setDefaultAttributes()
{
    setText(getLabel(), dontSendNotification);
    setJustificationType(Justification::centred);
    setColour(Label::textColourId, Colours::white);
    setColour(Label::backgroundColourId, Colours::purple.withAlpha(0.8f));

    setMinimumHorizontalScale(0.0f);
    //setInterceptsMouseClicks(false, false);
}

float LabelOverlayComponent::amplitudeToRelativeY(float amplitude)
{
    return jmin(1.0f, jmax(0.0f, 1 - (amplitude + 1) / 2));
}

float LabelOverlayComponent::frequencyToRelativeY(float frequency)
{
    return 0.0f; // TODO
}

float LabelOverlayComponent::pitchToRelativeY(float pitch)
{
    return jmin(1.0f, jmax(0.0f, 1 - pitch / 128));
}