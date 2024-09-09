#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

using namespace juce;


class LabelOverlayComponent : public Label
{
public:

    LabelOverlayComponent() {}
    LabelOverlayComponent(double t, String lbl);
    LabelOverlayComponent(double t, String lbl, double dur, String dsc);
    LabelOverlayComponent(const LabelOverlayComponent& other);
    virtual ~LabelOverlayComponent() = default;

    void paint(Graphics& g);

    void setTime(double t) { time = t; }
    void setLabel(String lbl) { label = lbl; }

    void setDuration(double dur) { duration = dur; }
    void setDescription(String d) { description = d; }

    float getRelativeY() const { return relativeY; }

    double getTime() const { return time; }
    String getLabel() const { return label; }
    double getDuration() const { return duration; }
    String getDescription() const { return description; }

protected:

    float relativeY;

    double time = 0.0;
    String label { "" };

    // Optional
    double duration;
    String description;
};

class AudioOverlayComponent : public LabelOverlayComponent
{
public:

    AudioOverlayComponent() {}
    AudioOverlayComponent(double t, String lbl);
    AudioOverlayComponent(const LabelOverlayComponent& other);
    AudioOverlayComponent(double t, String lbl, double dur, String dsc, float a);

    void setAmplitude(float a) { amplitude = a; }

    float getAmplitude() const { return amplitude; }

private:

    // Optional
    float amplitude;
};

class SpectrogramOverlayComponent : public LabelOverlayComponent
{
public:

    SpectrogramOverlayComponent() {}
    SpectrogramOverlayComponent(double t, String lbl);
    SpectrogramOverlayComponent(const LabelOverlayComponent& other);
    SpectrogramOverlayComponent(double t, String lbl, double dur, String dsc, float f);

    void setFrequency(float f) { frequency = f; }

    float getFrequency() const { return frequency; }

private:

    // Optional
    float frequency;
};

class MidiOverlayComponent : public LabelOverlayComponent
{
public:

    MidiOverlayComponent();
    MidiOverlayComponent(double t, String lbl);
    MidiOverlayComponent(const LabelOverlayComponent& other);
    MidiOverlayComponent(double t, String lbl, double dur, String dsc, float p);

    void setPitch(float p) { pitch = p; }

    float getPitch() const { return pitch; }

private:

    // Optional
    float pitch;
};
