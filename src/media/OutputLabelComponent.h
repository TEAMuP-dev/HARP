#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

using namespace juce;

class TimeMarkerComponent : public Component
{
public:
    TimeMarkerComponent(Colour clr = Colours::purple.withAlpha(0.8f))
    {
        color = clr;
    }

    void setColor(Colour clr) { color = clr; }

    void paint(juce::Graphics& g) override
    {
        g.setColour(color);
        g.fillRect(getLocalBounds());
    }

private:
    Colour color;
};

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
    void setColor(Colour clr);
    void setLink(String lnk) {link = lnk; }
    void setIndex(int i) {processingIndex = i; }

    double getTime() const { return time; }
    String getLabel() const { return label; }
    double getDuration() const { return duration; }
    String getDescription() const { return description; }
    Colour getColor() const { return color; }
    String getLink() const { return link; }
    int getIndex() const { return processingIndex; }

    float getTextWidth();

    juce::MouseCursor getMouseCursor() override;
    void mouseUp(const MouseEvent& e) override;
    void mouseEnter(const MouseEvent& e) override;
    void mouseExit(const MouseEvent& e) override;

    void setLeftMarkerBounds(Rectangle<int> b) { leftMarker.setBounds(b); }
    void setRightMarkerBounds(Rectangle<int> b)  { rightMarker.setBounds(b); }
    void setDurationFillBounds(Rectangle<int> b) { durationFill.setBounds(b); }

    void addMarkersTo(Component* c);
    void removeMarkersFrom(Component* c);
    void setMarkerVisibility(bool v);
    void setFillVisibility(bool v);

protected:
    double time;
    String label;

    // Optional
    double duration;
    String description;
    Colour color;
    String link;

private:
    int processingIndex = 0;

    TimeMarkerComponent leftMarker;
    TimeMarkerComponent rightMarker;
    TimeMarkerComponent durationFill;
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
