#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

using namespace juce;

class OutputLabelComponent : public Label
{
public:
    OutputLabelComponent(double t, String lbl, double dur = 0.0, String dsc = "", Colour clr = Colours::purple.withAlpha(0.8f), String lnk = "");
    OutputLabelComponent(const OutputLabelComponent& other);
    ~OutputLabelComponent();

    void setDefaultAttributes();

    void setTime(double t) { time = t; }
    void setLabel(String lbl) { label = lbl; }
    void setDuration(double dur) { duration = dur; }
    void setDescription(String d) { description = d; }
    void setColor(Colour clr) { color = clr; }
    void setLink(String lnk) {link = lnk; }

    double getTime() const { return time; }
    String getLabel() const { return label; }
    double getDuration() const { return duration; }
    String getDescription() const { return description; }
    Colour getColor() const { return color; }
    String getLink() const { return link; }

    juce::MouseCursor getMouseCursor() override;

protected:
    double time;
    String label;

    // Optional
    double duration;
    String description;
    Colour color;
    String link;
};

class OverheadLabelComponent : public OutputLabelComponent
{
public:
    OverheadLabelComponent(double t, String lbl, double dur, String dsc, Colour clr, String lnk);
    OverheadLabelComponent(const OverheadLabelComponent& other);
};

class LabelOverlayComponent : public OutputLabelComponent
{
public:
    LabelOverlayComponent(double t, String lbl, float y, double dur, String dsc, Colour clr, String lnk);
    LabelOverlayComponent(const LabelOverlayComponent& other);

    static float amplitudeToRelativeY(float amplitude);
    static float frequencyToRelativeY(float frequency);
    static float pitchToRelativeY(float pitch);

    void setRelativeY(float y) { relativeY = y; }

    float getRelativeY() const { return relativeY; }

private:
    float relativeY;
};
