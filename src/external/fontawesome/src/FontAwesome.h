/*
  ==============================================================================

    FontAwesome.h
    Created: 13 Jul 2014 12:19:09pm
    Author:  Daniel Lindenfelser

  ==============================================================================
*/

#pragma once
#include "juce_core/juce_core.h"
#include "juce_graphics/juce_graphics.h"
#include "juce_gui_basics/juce_gui_basics.h"
using namespace juce;

#include "../data/FontAwesomeData.h"
#include "../data/FontAwesomeIcons.h"

namespace fontawesome
{
typedef juce::Image RenderedIcon;

class IconHelper
{
public:
    IconHelper();
    virtual ~IconHelper();

    // std::unique_ptr<juce::Drawable>
    //     createIconDrawable(IconName icon, float size, juce::Colour colour, float scaleFactor);
    
    // std::unique_ptr<juce::Drawable> getDrawableFromImage(const juce::Image& image);

    // juce::Image createFontAwesomeImage(const IconName& iconName, float size, juce::Colour colour);
    
    // std::unique_ptr<juce::Drawable>
    //     getFontAwesomeIcon(const IconName& iconName, float size, juce::Colour colour);

    RenderedIcon getIcon(IconName icon, float size, juce::Colour colour, float scaleFactor = 1.0f);
    RenderedIcon getRotatedIcon(IconName icon,
                                float size,
                                juce::Colour colour,
                                float iconRotation,
                                float scaleFactor = 1.0f);

    void drawAt(juce::Graphics& g, RenderedIcon icon, int x, int y, float scaleFactor = 1.0f);
    void drawCenterdAt(juce::Graphics& g,
                       RenderedIcon icon,
                       juce::Rectangle<int> r,
                       float scaleFactor = 1.0f);

    juce::Font getFont();
    juce::Font getFont(float size);

    void drawAt(juce::Graphics& g,
                IconName icon,
                float size,
                juce::Colour colour,
                int x,
                int y,
                float scaleFactor);
    void drawCenterd(juce::Graphics& g,
                     IconName icon,
                     float size,
                     juce::Colour colour,
                     juce::Rectangle<int> r,
                     float scaleFactor);

    void drawAt(juce::Graphics& g, IconName icon, float size, juce::Colour colour, int x, int y);
    void drawCenterd(juce::Graphics& g,
                     IconName icon,
                     float size,
                     juce::Colour colour,
                     juce::Rectangle<int> r);

    void drawAtRotated(juce::Graphics& g,
                       IconName icon,
                       float size,
                       juce::Colour colour,
                       int x,
                       int y,
                       float rotation,
                       float scaleFactor);
    void drawCenterdRotated(juce::Graphics& g,
                            IconName icon,
                            float size,
                            juce::Colour colour,
                            juce::Rectangle<int> r,
                            float rotation,
                            float scaleFactor);

    void drawAtRotated(juce::Graphics& g,
                       IconName icon,
                       float size,
                       juce::Colour colour,
                       int x,
                       int y,
                       float rotation);
    void drawCenterdRotated(juce::Graphics& g,
                            IconName icon,
                            float size,
                            juce::Colour colour,
                            juce::Rectangle<int> r,
                            float rotation);

private:
    juce::Typeface::Ptr FontAwesome_ptr =
        juce::Typeface::createSystemTypefaceFor(FontAwesomeData::FontAwesomewebfont_ttf,
                                                FontAwesomeData::FontAwesomewebfont_ttfSize);

    // JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FontAwesome)
};

} // namespace fontawesome