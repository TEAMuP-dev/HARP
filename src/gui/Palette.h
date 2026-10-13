/**
 * @file Palette.h
 * @brief Colours and fonts of HARP's shared look, apart from the parts that present models.
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
} // namespace ModelStyle
