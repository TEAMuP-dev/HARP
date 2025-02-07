#include "OutputLabelComponent.h"

OutputLabelComponent::OutputLabelComponent(double t, String lbl, double dur, String dsc, Colour clr, String lnk)
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
}

void OutputLabelComponent::setColor(Colour clr)
{
    color = clr;

    leftMarker.setColor(clr);
    rightMarker.setColor(clr);
    durationFill.setColor(clr.withAlpha(0.5f));
}

float OutputLabelComponent::getTextWidth()
{
    return getFont().getStringWidthFloat(getText());
}

juce::MouseCursor OutputLabelComponent::getMouseCursor()
{
    if (link.isNotEmpty())
        return juce::MouseCursor::PointingHandCursor;
    else
        return juce::MouseCursor::NormalCursor;
}

void OutputLabelComponent::mouseUp(const juce::MouseEvent& e)
{
    String lnk = getLink();

    if (lnk.isNotEmpty()) {
        URL url = URL(lnk);

        if (url.isWellFormed()) {
            url.launchInDefaultBrowser();
        } else {
            DBG("OutputLabelComponent::mouseUp: label link \'" << lnk << "\' appears malformed.");
        }
    }
}

void OutputLabelComponent::mouseEnter(const juce::MouseEvent& e)
{
    setFillVisibility(true);
    setMarkerVisibility(true);
}

void OutputLabelComponent::mouseExit(const juce::MouseEvent& e)
{
    setFillVisibility(false);
    setMarkerVisibility(false);
}

void OutputLabelComponent::addMarkersTo(Component* c)
{
    c->addChildComponent(leftMarker);
    c->addChildComponent(rightMarker);
    c->addChildComponent(durationFill);
}

void OutputLabelComponent::removeMarkersFrom(Component* c)
{
    c->removeChildComponent(&leftMarker);
    c->removeChildComponent(&rightMarker);
    c->removeChildComponent(&durationFill);
}

void OutputLabelComponent::setMarkerVisibility(bool v)
{
    leftMarker.setVisible(v);
    rightMarker.setVisible(v);
}

void OutputLabelComponent::setFillVisibility(bool v)
{
    durationFill.setVisible(v);
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
