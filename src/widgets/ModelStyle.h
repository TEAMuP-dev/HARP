/**
 * @file ModelStyle.h
 * @brief Shared look for presenting models, on the Home tab and in model tabs alike: cards,
 *        tag chips, badges, and section headers.
 * @author cwitkowitz
 */

#pragma once

#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../gui/Palette.h"

#include "../utils/Interface.h"
#include "../utils/Messages.h"
#include "../utils/ModelCatalog.h"
#include "../utils/ModelTags.h"

using namespace juce;

namespace ModelStyle
{
/* Cards */

inline constexpr float cornerSize = 6.0f;

inline void
    drawCard(Graphics& g, Rectangle<float> bounds, bool isHighlighted = false, bool isDown = false)
{
    g.setColour(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::widgetBackground)
                    .brighter(isDown ? 0.14f : (isHighlighted ? 0.1f : 0.06f)));
    g.fillRoundedRectangle(bounds, cornerSize);

    g.setColour(isHighlighted ? accent.withAlpha(0.6f) : outline);
    g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
}

/* The lines text breaks into at a width, at most maxLines of them, the last of which takes
   the rest of the text. Line breaks in the text are treated as spaces. Laying text out is
   slow, so a caller that draws the same text repeatedly keeps the result. */
inline StringArray getTruncatedLines(const String& text, const Font& f, int width, int maxLines)
{
    auto words = StringArray::fromTokens(text, " \t\r\n", "");
    words.removeEmptyStrings();
    const String flattened = words.joinIntoString(" ");

    AttributedString attributed;
    attributed.append(flattened, f);

    TextLayout layout;
    layout.createLayout(attributed, (float) width);

    StringArray lines;

    const int numLines = jmin(maxLines, layout.getNumLines());

    for (int i = 0; i < numLines; ++i)
    {
        const auto range = layout.getLine(i).stringRange;

        lines.add((i == numLines - 1 ? flattened.substring(range.getStart())
                                     : flattened.substring(range.getStart(), range.getEnd()))
                      .trim());
    }

    return lines;
}

/* Draws lines from getTruncatedLines, which divide the area evenly, in the current font and
   colour, and ends the last line with an ellipsis if it does not fit. (drawFittedText shrinks
   the font instead, and ignores the limit altogether for text with line breaks.) */
inline void
    drawTruncatedLines(Graphics& g, const StringArray& lines, Rectangle<int> area, int maxLines)
{
    const int lineHeight = area.getHeight() / jmax(1, maxLines);

    for (int i = 0; i < lines.size(); ++i)
    {
        g.drawText(lines[i],
                   area.removeFromTop(lineHeight),
                   Justification::centredLeft,
                   i == lines.size() - 1);
    }
}

/* Tag chips */

inline constexpr int chipHeight = 18;
inline constexpr int chipGap = 4;

inline Font chipFont() { return font(11.0f, true); }

inline int getChipWidth(const String& text) { return getTextWidth(chipFont(), text) + 12; }

// Draws a rounded label at the given position and returns the x position just past it
inline int drawChip(Graphics& g, const String& text, int x, int y, Colour textColour, Colour fill)
{
    const auto bounds = Rectangle<int>(x, y, getChipWidth(text), chipHeight).toFloat();

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(textColour.withAlpha(0.45f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

    g.setColour(textColour);
    g.setFont(chipFont());
    g.drawText(text, bounds.toNearestInt(), Justification::centred, false);

    return x + (int) bounds.getWidth() + chipGap;
}

// Draws as many labels as fit in a row, followed by a count of the rest (e.g. "+2")
inline void drawTagRow(Graphics& g, const StringArray& labels, Rectangle<int> row)
{
    const int y = row.getCentreY() - chipHeight / 2;
    int x = row.getX();

    for (int i = 0; i < labels.size(); ++i)
    {
        const int remainingAfter = labels.size() - i - 1;
        const int reserved =
            remainingAfter > 0 ? chipGap + getChipWidth("+" + String(remainingAfter)) : 0;

        if (x + getChipWidth(labels[i]) + reserved > row.getRight())
        {
            drawChip(g,
                     "+" + String(labels.size() - i),
                     x,
                     y,
                     Colours::lightgrey,
                     Colours::transparentBlack);
            return;
        }

        x = drawChip(g, labels[i], x, y, chipText, chipBackground);
    }
}

// Everything the tags say, one line per aspect, for the instructions box
inline String describeTags(const ModelTags& tags)
{
    StringArray lines;

    // Each one listed separately here, where there is room for a model that takes
    // or returns several things
    if (! tags.inputs.empty())
        lines.add("Inputs: " + ModelTags::describe(tags.inputs));

    if (! tags.outputs.empty())
        lines.add("Outputs: " + ModelTags::describe(tags.outputs));

    // Spelled out as well as badged, since a bare identifier does not say it is a license
    if (const String terms = tags.describeLicense(); terms.isNotEmpty())
        lines.add("License: " + terms);

    if (const StringArray labels = tags.getDisplayLabels(); ! labels.isEmpty())
        lines.add("Tags: " + labels.joinIntoString(", "));

    return lines.joinIntoString("\n");
}

/* Badges */

struct Badge
{
    String text;
    Colour colour;
};

/* Notes on a model's deployment. Whether it can be expected to load is only worth noting
   before it is loaded, so that is left out once it has been. */
inline std::vector<Badge> getBadges(const CatalogEntry& entry, bool includeAvailability = true)
{
    std::vector<Badge> badges;

    if (includeAvailability)
    {
        if (const auto note = entry.getAvailabilityNote(); note.text.isNotEmpty())
            badges.push_back({ note.text, note.isProblem ? problem : information });
    }

    if (entry.isZeroGPU())
        badges.push_back({ "ZeroGPU", zeroGPU });

    if (entry.provider != "Hugging Face")
        badges.push_back({ entry.provider, Colours::lightgrey });

    /* Last, so that it is drawn leftmost and reads straight after the name. The
       availability note and the hardware stay at the right edge, where a reader
       who is scanning a column of cards expects to find them. */
    if (const String terms = entry.tags.describeLicense(); terms.isNotEmpty())
        badges.push_back({ terms, license });

    return badges;
}

inline Font badgeFont() { return font(11.0f, true); }

inline int getBadgeWidth(const Badge& badge) { return getTextWidth(badgeFont(), badge.text) + 12; }

// Left edge of the badges drawBadges would draw in a row, or its right edge if there are none
inline int getBadgesLeft(const std::vector<Badge>& badges, Rectangle<int> row)
{
    int left = row.getRight() + chipGap;

    for (const auto& badge : badges)
        left -= getBadgeWidth(badge) + chipGap;

    return left - (badges.empty() ? chipGap : 0);
}

// Draws badges from the right of a row leftwards, and returns the left edge of the last one
inline int drawBadges(Graphics& g, const std::vector<Badge>& badges, Rectangle<int> row)
{
    int right = row.getRight();

    for (const auto& badge : badges)
    {
        const int width = getBadgeWidth(badge);
        const auto bounds = Rectangle<int>(right - width, row.getCentreY() - 9, width, 18);

        g.setColour(badge.colour.withAlpha(0.18f));
        g.fillRoundedRectangle(bounds.toFloat(), 9.0f);
        g.setColour(badge.colour);
        g.setFont(badgeFont());
        g.drawText(badge.text, bounds, Justification::centred, false);

        right = bounds.getX() - chipGap;
    }

    return right + (badges.empty() ? 0 : chipGap);
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

/**
 * A row of tag chips, which describes all of the tags in the instructions box while
 * hovering over it (including any that did not fit).
 */
class TagRow : public Component
{
public:
    void setTags(const ModelTags& newTags)
    {
        tags = newTags;
        labels = tags.getDisplayLabels();

        repaint();
    }

    bool isEmpty() const { return labels.isEmpty(); }

    void paint(Graphics& g) override { drawTagRow(g, labels, getLocalBounds()); }

    void mouseEnter(const MouseEvent&) override
    {
        instructionsMessage->setMessage(describeTags(tags));
    }

    void mouseExit(const MouseEvent&) override { instructionsMessage->clearMessage(); }

private:
    ModelTags tags;
    StringArray labels;

    SharedResourcePointer<InstructionsMessage> instructionsMessage;
};
} // namespace ModelStyle
