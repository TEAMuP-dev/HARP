/**
 * @file ModelInfoWidget.h
 * @brief Component presenting model metadata and information.
 * @author cwitkowitz, xribene, hugofloresgarcia
 */

#pragma once

#include <cmath>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../widgets/ModelStyle.h"
#include "../widgets/StatusAreaWidget.h"

#include "../gui/HoverHandler.h"
#include "../gui/HoverableLabel.h"

#include "../utils/Logging.h"

using namespace juce;

class ModelAuthorLabel : public Component
{
public:
    ModelAuthorLabel()
    {
        modelLabel.setFont(ModelStyle::font(18.0f, true));
        modelLabel.setBorderSize({ 0, 0, 0, 0 });

        modelLabel.onHover = [this]
        { instructionsMessage->setMessage("Click to view the model's webpage or documentation."); };
        modelLabel.onExit = [this] { instructionsMessage->clearMessage(); };
        modelLabel.onClick = [this] { url.launchInDefaultBrowser(); };

        authorLabel.setFont(ModelStyle::font(13.0f));
        authorLabel.setColour(Label::textColourId, ModelStyle::secondaryText);

        setURL(URL());

        addAndMakeVisible(modelLabel);
        addAndMakeVisible(authorLabel);
    }

    void resized() override
    {
        Rectangle<int> totalArea = getLocalBounds();

        const int modelNameWidth =
            ModelStyle::getTextWidth(modelLabel.getFont(), modelLabel.getText()) + 2;

        modelLabel.setBounds(totalArea.removeFromLeft(jmin(modelNameWidth, totalArea.getWidth())));
        authorLabel.setBounds(totalArea.withTrimmedTop(2));
    }

    void setModelName(const String& modelName)
    {
        modelLabel.setText(modelName, dontSendNotification);

        resized();
    }

    void setAuthor(const String& author)
    {
        authorLabel.setText(author, dontSendNotification);

        resized();
    }

    void setURL(const URL& newURL)
    {
        const bool isLinked = newURL.isWellFormed() && newURL.toString(false).isNotEmpty();

        url = newURL;

        modelLabel.setHoverColor(isLinked ? ModelStyle::accent : Colours::white);
        modelLabel.setHoverable(isLinked);
    }

private:
    HoverableLabel modelLabel;
    Label authorLabel;

    URL url;

    SharedResourcePointer<InstructionsMessage> instructionsMessage;
};

/**
 * The loaded model's card, presented like the Home tab's: its name (linking to its page)
 * and author, notes on its deployment, its description, and its tags.
 */
class ModelInfoWidget : public Component
{
public:
    ModelInfoWidget()
    {
        addAndMakeVisible(modelAuthorLabel);

        // Scrollable read-only text, for descriptions longer than the widget shows
        description.setMultiLine(true);
        description.setReadOnly(true);
        description.setScrollbarsShown(true);
        description.setCaretVisible(false);
        description.setPopupMenuEnabled(false);
        description.setFont(descriptionFont);
        description.setIndents(0, 0);
        description.setColour(TextEditor::textColourId, Colours::whitesmoke.withAlpha(0.85f));
        description.setColour(TextEditor::backgroundColourId, Colours::transparentBlack);
        description.setColour(TextEditor::outlineColourId, Colours::transparentBlack);
        description.setColour(TextEditor::focusedOutlineColourId, Colours::transparentBlack);
        description.setColour(TextEditor::shadowColourId, Colours::transparentBlack);
        addAndMakeVisible(description);

        addChildComponent(tagRow);
    }

    void paint(Graphics& g) override
    {
        ModelStyle::drawCard(g, getLocalBounds().toFloat().reduced(1.0f));
        ModelStyle::drawBadges(g, badges, getContentArea().removeFromTop(headerHeight));
    }

    void resized() override
    {
        auto area = getContentArea();

        auto headerRow = area.removeFromTop(headerHeight);
        headerRow.setRight(ModelStyle::getBadgesLeft(badges, headerRow) - ModelStyle::chipGap);
        modelAuthorLabel.setBounds(headerRow);

        area.removeFromTop(rowGap);
        description.setBounds(area.removeFromTop(getDescriptionHeightForWidth(area.getWidth())));

        if (! tagRow.isEmpty())
        {
            area.removeFromTop(rowGap + 2);
            tagRow.setBounds(area.removeFromTop(ModelStyle::chipHeight));
        }
    }

    int getPreferredHeightForWidth(int width) const
    {
        const int contentWidth = width - 2 * horizontalPadding;

        int height = 2 * verticalPadding + headerHeight + rowGap
                     + getDescriptionHeightForWidth(contentWidth);

        if (! tagRow.isEmpty())
            height += rowGap + 2 + ModelStyle::chipHeight;

        return height;
    }

    void updateLabels(const ModelMetadata& metadata)
    {
        modelAuthorLabel.setModelName(String(metadata.name));
        modelAuthorLabel.setAuthor(metadata.author.empty() ? String()
                                                           : "by " + String(metadata.author));

        description.setText(String(metadata.description));

        StringArray tags;

        for (const auto& tag : metadata.tags)
            tags.add(tag);

        tagRow.setTags(ModelTags::parse(tags));
        tagRow.setVisible(! tagRow.isEmpty());

        resized();
        repaint();
    }

    void setBadges(std::vector<ModelStyle::Badge> newBadges)
    {
        badges = std::move(newBadges);

        resized();
        repaint();
    }

    void addOpenablePath(const String& openablePath) { modelAuthorLabel.setURL(URL(openablePath)); }

private:
    Rectangle<int> getContentArea() const
    {
        return getLocalBounds().reduced(horizontalPadding, verticalPadding);
    }

    // Tall enough for the description, up to a limit beyond which it scrolls
    int getDescriptionHeightForWidth(int width) const
    {
        if (description.isEmpty() || width <= 0)
            return 0;

        AttributedString text;
        text.append(description.getText(), descriptionFont);

        TextLayout layout;
        // Leave room for the scrollbar, which appears once the text is too long
        layout.createLayout(text, (float) (width - getLookAndFeel().getDefaultScrollbarWidth()));

        const int lines = jlimit(1, maxDescriptionLines, layout.getNumLines());

        return (int) std::ceil((float) lines * descriptionFont.getHeight()) + 2;
    }

    static constexpr int horizontalPadding = 12;
    static constexpr int verticalPadding = 9;
    static constexpr int headerHeight = 24;
    static constexpr int rowGap = 2;
    static constexpr int maxDescriptionLines = 4;

    const Font descriptionFont = ModelStyle::font(13.0f);

    ModelAuthorLabel modelAuthorLabel;
    TextEditor description;
    ModelStyle::TagRow tagRow;

    std::vector<ModelStyle::Badge> badges;
};
