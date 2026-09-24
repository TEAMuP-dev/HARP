/**
 * @file ModelInfoWidget.h
 * @brief Component presenting model metadata and information.
 * @author hugofloresgarcia, xribene, cwitkowitz
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

#include "../widgets/StatusAreaWidget.h"

#include "../gui/HoverHandler.h"
#include "../gui/HoverableLabel.h"

#include "../utils/Logging.h"

using namespace juce;

class ModelAuthorLabel : public Component
{
public:
    ModelAuthorLabel(const String& modelName = "",
                     const String& author = "",
                     const URL& newURL = URL())
    {
        if (modelName.isNotEmpty())
        {
            setModelName(modelName);
        }

        if (author.isNotEmpty())
        {
            setAuthor(author);
        }

        setURL(newURL);

        modelLabel.setFont(Font(22.0f, Font::bold));

        modelLabel.onHover = [this]
        { instructionsMessage->setMessage("Click to view the model's webpage or documentation."); };
        modelLabel.onExit = [this] { instructionsMessage->clearMessage(); };
        modelLabel.onClick = [this] { url.launchInDefaultBrowser(); };

        addAndMakeVisible(modelLabel);
        addAndMakeVisible(authorLabel);
    }

    void resized() override
    {
        Rectangle<int> totalArea = getLocalBounds();

        float modelNameWidth = modelLabel.getFont().getStringWidthFloat(modelLabel.getText()) + 10;

        modelLabel.setBounds(totalArea.removeFromLeft(static_cast<int>(modelNameWidth)));
        authorLabel.setBounds(totalArea.translated(0, 3));
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
        if (newURL.isWellFormed())
        {
            url = newURL;

            modelLabel.setHoverColor(Colours::coral);
            modelLabel.setHoverable(true);

            resized();
        }
        else
        {
            modelLabel.setHoverColor(Colours::white);
            modelLabel.setHoverable(false);
        }
    }

private:
    HoverableLabel modelLabel;
    Label authorLabel;

    URL url;

    SharedResourcePointer<InstructionsMessage> instructionsMessage;
};

class ModelInfoWidget : public Component
{
public:
    ModelInfoWidget()
    {
        addAndMakeVisible(modelAuthorLabel);

        // Configure description as scrollable read-only text
        description.setMultiLine(true);
        description.setReadOnly(true);
        description.setScrollbarsShown(true);
        description.setCaretVisible(false); // no blinking cursor
        description.setPopupMenuEnabled(false); // disable right-click menu
        description.setFont(Font(15.0f));

        // Make it visually match the old TextLabel appearance
        description.setColour(TextEditor::backgroundColourId, Colours::transparentBlack);
        description.setColour(TextEditor::outlineColourId, Colours::transparentBlack);
        description.setColour(TextEditor::shadowColourId, Colours::transparentBlack);

        addAndMakeVisible(description);
    }

    ~ModelInfoWidget() {}

    //void paint(Graphics& g) {}

    void resized() override
    {
        Rectangle<int> bounds = getLocalBounds().reduced(marginSize);

        // Set fixed size for model and author labels
        modelAuthorLabel.setBounds(bounds.removeFromTop(headerHeight));
        // Grant remaining space to description
        description.setBounds(bounds);
    }

    int getPreferredHeightForWidth(int width) const
    {
        const int contentWidth = jmax(120, width - 2 * (int) marginSize);
        const int lineCount = estimateWrappedLineCount(description.getText(), contentWidth);
        const int visibleLines = jlimit(1, 4, lineCount);
        const int lineHeight = (int) std::ceil(description.getFont().getHeight() + 2.0f);
        const int descriptionHeight = visibleLines * lineHeight + 4;

        return (int) (2 * marginSize + headerHeight + descriptionHeight);
    }

    void resetState()
    {
        ModelMetadata emptyMetadata;

        updateLabels(emptyMetadata);
        modelAuthorLabel.setURL(URL(""));
    }

    void updateLabels(const ModelMetadata& metadata)
    {
        if (metadata.name.empty())
        {
            modelAuthorLabel.setModelName("");
        }
        else
        {
            modelAuthorLabel.setModelName(String(metadata.name));
        }

        if (metadata.author.empty())
        {
            modelAuthorLabel.setAuthor("");
        }
        else
        {
            modelAuthorLabel.setAuthor("by " + String(metadata.author));
        }

        if (metadata.description.empty())
        {
            description.setText("");
        }
        else
        {
            description.setText(String(metadata.description));
        }

        resized();
    }

    void addOpenablePath(const String& openablePath) { modelAuthorLabel.setURL(URL(openablePath)); }

private:
    int estimateWrappedLineCount(const String& text, int availableWidth) const
    {
        if (availableWidth <= 0)
            return 1;

        auto font = description.getFont();
        const float spaceWidth = jmax(1.0f, font.getStringWidthFloat(" "));
        int lines = 0;

        StringArray paragraphs;
        paragraphs.addLines(text.isEmpty() ? String(" ") : text);

        for (const auto& paragraph : paragraphs)
        {
            StringArray words;
            words.addTokens(paragraph, " ", "");
            words.removeEmptyStrings();

            if (words.isEmpty())
            {
                ++lines;
                continue;
            }

            float currentLineWidth = 0.0f;
            for (const auto& word : words)
            {
                const float wordWidth = font.getStringWidthFloat(word);

                if (currentLineWidth <= 0.0f)
                {
                    currentLineWidth = wordWidth;
                    continue;
                }

                if (currentLineWidth + spaceWidth + wordWidth <= (float) availableWidth)
                {
                    currentLineWidth += spaceWidth + wordWidth;
                }
                else
                {
                    ++lines;
                    currentLineWidth = wordWidth;
                }
            }

            ++lines;
        }

        return jmax(1, lines);
    }

    const float headerHeight = 30;
    const float marginSize = 2;

    ModelAuthorLabel modelAuthorLabel;

    TextEditor description;
};
