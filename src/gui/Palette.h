/**
 * @file Palette.h
 * @brief Colours, fonts, and section headers of HARP's shared look, apart from the parts that
 *        present models.
 * @author cwitkowitz
 */

#pragma once

#include <map>
#include <utility>

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

/* This opens the ModelStyle namespace, which widgets/ModelStyle.h continues with cards,
   chips, and section headers. The two are kept apart so that components below the
   widgets, such as tracks, can share the colours without depending on the catalog. */
namespace ModelStyle
{
inline const Colour accent { 0xff2dd4bf };
inline const Colour accentDark { 0xff0f766e };
inline const Colour chipBackground { 0xff183238 };
inline const Colour chipText { 0xff9eeadf };
inline const Colour problem { 0xfff87171 };
inline const Colour information { 0xff7dd3fc };
inline const Colour zeroGPU { 0xffa78bfa };
/* A license sits with the badges rather than the tags because it answers the
   same kind of question they do: not what the model is, but what using it
   entails. Amber reads as something to note without reading as an error. */
inline const Colour license { 0xffe9c894 };
inline const Colour secondaryText { 0xffa3a3a3 };
// Outline of a card, or of any panel edge, when it is not highlighted
inline const Colour outline = Colours::white.withAlpha(0.12f);

inline Font font(float height, bool bold = false)
{
    return Font(FontOptions(height, bold ? Font::bold : Font::plain));
}

/* Measures text once for each font and string. JUCE shapes the text on every measurement,
   which is slow enough to show in paint() and resized(), where most of these are taken.
   Only the message thread measures text, so the cache needs no lock. */
inline int getTextWidth(const Font& f, const String& text)
{
    static std::map<std::pair<String, String>, int> widths;

    const auto key = std::make_pair(f.toString(), text);

    auto it = widths.find(key);

    if (it == widths.end())
    {
        it = widths.emplace(key, GlyphArrangement::getStringWidthInt(f, text)).first;
    }

    return it->second;
}

/* Section headers */

inline constexpr int sectionHeaderHeight = 26;

// Draws a heading followed by a rule to the end of the area, or only the rule where the
// heading does not fit, since a heading cut short is harder to read than none at all
inline void drawSectionHeader(Graphics& g, const String& title, Rectangle<int> area)
{
    const Font titleFont = font(15.0f, true);
    const int titleWidth = getTextWidth(titleFont, title);

    int lineX = area.getX();

    if (titleWidth <= area.getWidth())
    {
        g.setColour(Colours::white);
        g.setFont(titleFont);
        g.drawText(title, area, Justification::centredLeft, false);

        lineX += titleWidth + 10;
    }

    g.setColour(accent.withAlpha(0.5f));
    g.fillRect(lineX, area.getCentreY(), jmax(0, area.getRight() - lineX), 1);
}

class SectionHeader : public Component
{
public:
    explicit SectionHeader(const String& headerTitle) : title(headerTitle)
    {
        setInterceptsMouseClicks(false, false);
    }

    void paint(Graphics& g) override { drawSectionHeader(g, title, getLocalBounds()); }

private:
    const String title;
};
} // namespace ModelStyle
