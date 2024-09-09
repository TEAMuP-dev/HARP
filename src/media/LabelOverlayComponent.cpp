#include "LabelOverlayComponent.h"


LabelOverlayComponent::LabelOverlayComponent(double t, String lbl)
{
    setTime(t);
    setLabel(lbl);
}

LabelOverlayComponent::LabelOverlayComponent(double t, String lbl, double dur, String dsc)
{
    LabelOverlayComponent(t, lbl);

    setDuration(dur);
    setDescription(dsc);
}

LabelOverlayComponent::LabelOverlayComponent(const LabelOverlayComponent& other)
{
    setTime(other.getTime());
    setLabel(other.getLabel());
    setDuration(other.getDuration());
    setDescription(other.getDescription());
}

void LabelOverlayComponent::paint(Graphics& g)
{
    g.fillAll(Colours::purple);
}

AudioOverlayComponent::AudioOverlayComponent(const LabelOverlayComponent& other)
{
    setTime(other.getTime());
    setLabel(other.getLabel());
    setDuration(other.getDuration());
    setDescription(other.getDescription());
}

AudioOverlayComponent::AudioOverlayComponent(double t, String lbl, double dur, String dsc, float a)
{
    setTime(t);
    setLabel(lbl);

    setDuration(dur);
    setDescription(dsc);

    setAmplitude(a);
}

SpectrogramOverlayComponent::SpectrogramOverlayComponent(const LabelOverlayComponent& other)
{
    setTime(other.getTime());
    setLabel(other.getLabel());
    setDuration(other.getDuration());
    setDescription(other.getDescription());
}

SpectrogramOverlayComponent::SpectrogramOverlayComponent(double t, String lbl, double dur, String dsc, float f)
{
    setTime(t);
    setLabel(lbl);

    setDuration(dur);
    setDescription(dsc);

    setFrequency(f);
}

MidiOverlayComponent::MidiOverlayComponent(const LabelOverlayComponent& other)
{
    setTime(other.getTime());
    setLabel(other.getLabel());
    setDuration(other.getDuration());
    setDescription(other.getDescription());
}

MidiOverlayComponent::MidiOverlayComponent(double t, String lbl, double dur, String dsc, float p)
{
    setTime(t);
    setLabel(lbl);

    setDuration(dur);
    setDescription(dsc);

    setPitch(p);
}
