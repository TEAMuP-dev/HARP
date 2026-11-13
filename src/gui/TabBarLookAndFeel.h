/**
 * @file TabBarLookAndFeel.h
 * @brief The look of a tab bar, shared by the model tabs and the settings window.
 * @author cwitkowitz, VedMistry42
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

class TabBarLookAndFeel : public LookAndFeel_V4
{
public:
    void drawTabbedButtonBarBackground(TabbedButtonBar& bar, Graphics& g) override
    {
        g.fillAll(tabBarColour);
        g.setColour(separatorColour);
        g.fillRect(0, bar.getHeight() - 1, bar.getWidth(), 1);
    }

    void drawTabAreaBehindFrontButton(TabbedButtonBar&, Graphics& g, int w, int h) override
    {
        g.setColour(separatorColour);
        g.fillRect(0, h - 1, w, 1);
    }

    void drawTabButton(TabBarButton& button,
                       Graphics& g,
                       bool isMouseOver,
                       bool isMouseDown) override
    {
        const auto isActive = button.isFrontTab();
        auto area = button.getActiveArea();

        const auto fill =
            isActive ? activeTabColour
                     : inactiveTabColour.brighter(isMouseOver || isMouseDown ? 0.08f : 0.0f);

        g.setColour(fill);
        g.fillRect(area);

        if (button.getIndex() > 0)
        {
            g.setColour(separatorColour);
            g.fillRect(area.getX(), area.getY() + 2, 1, area.getHeight() - 4);
        }

        auto textArea = button.getTextArea().reduced(tabTextInset, 0);

        g.setColour(isActive ? activeTextColour : inactiveTextColour);

        g.drawText(button.getButtonText(), textArea, Justification::centred, true);
    }

    // Every tab is as wide as the next, however long its name
    int getTabButtonBestWidth(TabBarButton& /*button*/, int /*tabDepth*/) override
    {
        return tabWidth;
    }

    /* The default gives a tab's close button the full height of the tab, which stretches it,
       so it is kept square, centred, and clear of the tab's edge instead */
    Rectangle<int> getTabButtonExtraComponentBounds(const TabBarButton& button,
                                                    Rectangle<int>& textArea,
                                                    Component& extraComponent) override
    {
        textArea.removeFromRight(extraComponentMargin);

        const int side = extraComponent.getWidth();

        return LookAndFeel_V4::getTabButtonExtraComponentBounds(button, textArea, extraComponent)
            .withSizeKeepingCentre(side, side);
    }

    void drawTabButtonText(TabBarButton&,
                           Graphics&,
                           bool /*isMouseOver*/,
                           bool /*isMouseDown*/) override
    {
    }

protected:
    static constexpr int tabWidth = 140;

private:
    const Colour tabBarColour { Colour(0xff1f1f1f) };
    const Colour inactiveTabColour { Colour(0xff242424) };
    const Colour activeTabColour { Colour(0xff343434) };
    const Colour separatorColour { Colour(0xff4a4a4a) };
    const Colour activeTextColour { Colours::white };
    const Colour inactiveTextColour { Colour(0xffaeb0b4) };
    static constexpr int tabTextInset = 10;
    static constexpr int extraComponentMargin = 6;
};
