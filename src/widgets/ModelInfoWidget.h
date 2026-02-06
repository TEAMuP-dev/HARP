/**
 * @file ModelInfoWidget.h
 * @brief Component presenting model metadata and information.
 * @author hugofloresgarcia, xribene, cwitkowitz
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

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

    // void paint(Graphics& g) override {}

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

            modelLabel.setHoverColour(Colours::coral);
            modelLabel.setHoverable(true);

            resized();
        }
        else
        {
            modelLabel.setHoverColour(Colours::white);
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

        description.setFont(Font(15.0f));
        description.setJustificationType(Justification::topLeft);
        description.setInterceptsMouseClicks(true, false);

        descriptionViewport.setViewedComponent(&description, false);
        descriptionViewport.setScrollBarsShown(true, false);

        addAndMakeVisible(descriptionViewport);
    }

    ~ModelInfoWidget() {}

    //void paint(Graphics& g) {}

    void resized() override
    {
        Rectangle<int> bounds = getLocalBounds().reduced(marginSize);

        // Set fixed size for model and author labels
        modelAuthorLabel.setBounds(bounds.removeFromTop(headerHeight));
        // Grant remaining space to description
        descriptionViewport.setBounds(bounds);

        const int availableWidth = descriptionViewport.getWidth();

        if (availableWidth > 0)
        {
            AttributedString attributedText;
            attributedText.setText(description.getText());
            attributedText.setFont(description.getFont());
            attributedText.setJustification(Justification::topLeft);

            TextLayout layout;
            layout.createLayout(attributedText, static_cast<float>(availableWidth));

            const int textHeight = static_cast<int>(std::ceil(layout.getHeight()));

            description.setSize(availableWidth, textHeight);
        }
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
            description.setText("", dontSendNotification);
        }
        else
        {
            description.setText(String(metadata.description), dontSendNotification);
        }

        resized();
    }

    void addOpenablePath(const String& openablePath) { modelAuthorLabel.setURL(URL(openablePath)); }

private:
    const float headerHeight = 30;
    const float marginSize = 2;

    ModelAuthorLabel modelAuthorLabel;

    Label description;
    Viewport descriptionViewport;
};
