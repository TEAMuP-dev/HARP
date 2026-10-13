/**
 * @file RoundIconButton.h
 * @brief Small round buttons drawn from strokes, for closing, removing, or minimizing
 *        whatever they sit on.
 * @author cwitkowitz, 2cylu2
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../utils/Messages.h"

using namespace juce;

/**
 * Highlights a circle behind its icon while hovered, and explains itself in the
 * instructions box, since an icon alone does not say what it closes or removes.
 */
class RoundIconButton : public Button
{
public:
    explicit RoundIconButton(const String& instructionsText)
        : Button("RoundIcon"), instructions(instructionsText)
    {
    }

    void setInstructions(const String& newInstructions)
    {
        instructions = newInstructions;

        if (isMouseOver())
            instructionsMessage->setMessage(instructions);
    }

    void paintButton(Graphics& g, bool isHighlighted, bool isDown) override
    {
        // Square whatever this is given, so that the circle and icon are never stretched
        const float side = (float) jmin(getWidth(), getHeight());
        const auto bounds =
            getLocalBounds().toFloat().withSizeKeepingCentre(side, side).reduced(1.0f);

        if (isHighlighted || isDown)
        {
            g.setColour(Colours::white.withAlpha(isDown ? 0.2f : 0.12f));
            g.fillEllipse(bounds);
        }

        g.setColour(isHighlighted ? Colours::white : Colours::lightgrey);
        drawIcon(g, bounds.reduced(bounds.getWidth() * 0.32f));
    }

    void mouseEnter(const MouseEvent& e) override
    {
        Button::mouseEnter(e);
        instructionsMessage->setMessage(instructions);
    }

    void mouseExit(const MouseEvent& e) override
    {
        Button::mouseExit(e);
        instructionsMessage->clearMessage();
    }

protected:
    // Strokes, which sit centred where a glyph might not, in the current colour
    virtual void drawIcon(Graphics& g, Rectangle<float> area) = 0;

    static constexpr float strokeWidth = 1.5f;

private:
    String instructions;

    SharedResourcePointer<InstructionsMessage> instructionsMessage;
};

class CloseButton : public RoundIconButton
{
public:
    using RoundIconButton::RoundIconButton;

private:
    void drawIcon(Graphics& g, Rectangle<float> area) override
    {
        g.drawLine({ area.getTopLeft(), area.getBottomRight() }, strokeWidth);
        g.drawLine({ area.getBottomLeft(), area.getTopRight() }, strokeWidth);
    }
};

class ChevronButton : public RoundIconButton
{
public:
    using RoundIconButton::RoundIconButton;

    void setPointingUp(bool shouldPointUp)
    {
        pointingUp = shouldPointUp;
        repaint();
    }

private:
    void drawIcon(Graphics& g, Rectangle<float> area) override
    {
        // Half as tall as it is wide, centred in the area
        const float halfHeight = area.getWidth() / 4.0f;
        const float tipY = area.getCentreY() + (pointingUp ? -halfHeight : halfHeight);
        const float endY = area.getCentreY() + (pointingUp ? halfHeight : -halfHeight);

        Path chevron;
        chevron.startNewSubPath(area.getX(), endY);
        chevron.lineTo(area.getCentreX(), tipY);
        chevron.lineTo(area.getRight(), endY);

        g.strokePath(chevron,
                     PathStrokeType(strokeWidth, PathStrokeType::curved, PathStrokeType::rounded));
    }

    bool pointingUp = false;
};
