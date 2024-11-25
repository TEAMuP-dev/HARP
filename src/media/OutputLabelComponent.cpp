#include "OutputLabelComponent.h"

OutputLabelComponent::OutputLabelComponent(double t,
                                           String lbl,
                                           double dur = 0.0,
                                           String dsc = "",
                                           Colour clr = Colours::purple.withAlpha(0.8f),
                                           String lnk = "")
{
    setTime(t);
    setLabel(lbl);
    setDuration(dur);

    if (dsc.isEmpty())
    {
        dsc = lbl;
    }

    setDescription(dsc);
    setColor(clr);
    setLink(lnk);

    setDefaultAttributes();
}

OutputLabelComponent::OutputLabelComponent(const OutputLabelComponent& other)
{
    setTime(other.getTime());
    setLabel(other.getLabel());
    setDuration(other.getDuration());
    setDescription(other.getDescription());
    setColor(other.getColor());
    setLink(other.getLink());

    setDefaultAttributes();
}

OutputLabelComponent::~OutputLabelComponent() {}

void OutputLabelComponent::setDefaultAttributes()
{
    setText(getLabel(), dontSendNotification);
    setJustificationType(Justification::centred);
    setColour(Label::textColourId, Colours::white);
    setColour(Label::backgroundColourId, getColor());

    setMinimumHorizontalScale(0.0f);
    //setInterceptsMouseClicks(false, false);
}

juce::MouseCursor OutputLabelComponent::getMouseCursor()
{
    if (link.isNotEmpty())
        return juce::MouseCursor::PointingHandCursor;
    else
        return juce::MouseCursor::NormalCursor;
}

OverheadLabelComponent::OverheadLabelComponent(double t,
                                             String lbl,
                                             double dur,
                                             String dsc,
                                             Colour clr,
                                             String lnk)
    : OutputLabelComponent(t, lbl, dur, dsc, clr, lnk) {}

OverheadLabelComponent::OverheadLabelComponent(const OverheadLabelComponent& other)
    : OutputLabelComponent(other) {}

LabelOverlayComponent::LabelOverlayComponent(double t,
                                             String lbl,
                                             float y,
                                             double dur,
                                             String dsc,
                                             Colour clr,
                                             String lnk)
    : OutputLabelComponent(t, lbl, dur, dsc, clr, lnk)
{
    setRelativeY(y);
}

LabelOverlayComponent::LabelOverlayComponent(const LabelOverlayComponent& other)
    : OutputLabelComponent(other)
{
    setRelativeY(other.getRelativeY());
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
