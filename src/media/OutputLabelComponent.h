#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

using namespace juce;


class OutputLabelComponent : public Label
{
    // TODO - base class for labels
};


class OverheadLabelComponent : public OutputLabelComponent
{
    // TODO - labels without specified height (display above media)
};


class LabelOverlayComponent : public OutputLabelComponent
{
public:

    LabelOverlayComponent(double t, String lbl, float y);
    LabelOverlayComponent(double t, String lbl, float y, double dur);
    LabelOverlayComponent(double t, String lbl, float y, String dsc);
    LabelOverlayComponent(double t, String lbl, float y, double dur, String dsc);
    LabelOverlayComponent(double t, String lbl, float y, double dur, String dsc, Colour color);
    LabelOverlayComponent(const LabelOverlayComponent& other);
    ~LabelOverlayComponent();

    void setDefaultAttributes();

    static float amplitudeToRelativeY(float amplitude);
    static float frequencyToRelativeY(float frequency);
    static float pitchToRelativeY(float pitch);

    void setTime(double t) { time = t; }
    void setLabel(String lbl) { label = lbl; }

    void setRelativeY(float y) { relativeY = y; }

    void setDuration(double dur) { duration = dur; }
    void setDescription(String d) { description = d; }
    void setColor(Colour clr) {color = clr; }

    double getTime() const { return time; }
    String getLabel() const { return label; }

    float getRelativeY() const { return relativeY; }

    double getDuration() const { return duration; }
    String getDescription() const { return description; }
    Colour getColor() const { return color; }

protected:

    double time;
    String label;

    float relativeY;

    // Optional
    double duration = 0.0;
    String description { "" };
    Colour color = Colours::purple.withAlpha(0.8f);
};
