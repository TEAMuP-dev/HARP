/**
 * @file AboutWindow.h
 * @brief Window containing basic information and links.
 * @author cwitkowitz, hugofloresgarcia
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../gui/Palette.h"

#include "../utils/Interface.h"

using namespace juce;

class AboutWindow : public Component
{
public:
    AboutWindow()
    {
        nameLabel.setText(APP_NAME, dontSendNotification);
        nameLabel.setFont(ModelStyle::font(24.0f, true));
        addAndMakeVisible(nameLabel);

        versionLabel.setText("Version " + String(APP_VERSION), dontSendNotification);
        versionLabel.setFont(ModelStyle::font(14.0f));
        versionLabel.setColour(Label::textColourId, ModelStyle::secondaryText);
        addAndMakeVisible(versionLabel);

        descriptionLabel.setText(
            "Hosted, asynchronous, remote processing of audio and MIDI with machine learning.",
            dontSendNotification);
        descriptionLabel.setFont(ModelStyle::font(14.0f));
        descriptionLabel.setMinimumHorizontalScale(1.0f);
        addAndMakeVisible(descriptionLabel);

        addAndMakeVisible(linksHeader);

        addLink(websiteLink, "Visit the HARP website", "https://harp.tools/");
        addLink(
            modelGlossaryLink, "Browse the model glossary", "https://harp.tools/usage/models.html");
        addLink(reportIssueLink, "Report an issue", "https://github.com/TEAMuP-dev/HARP/issues");

        addAndMakeVisible(acknowledgementsHeader);

        acknowledgementsLabel.setText("Built with JUCE. Icons by Font Awesome and Fontaudio.",
                                      dontSendNotification);
        acknowledgementsLabel.setFont(ModelStyle::font(14.0f));
        acknowledgementsLabel.setMinimumHorizontalScale(1.0f);
        addAndMakeVisible(acknowledgementsLabel);

        addLink(noticesLink,
                "Read the third-party notices",
                "https://github.com/TEAMuP-dev/HARP/blob/main/THIRD_PARTY_NOTICES.md");

        copyrightLabel.setText(APP_COPYRIGHT, dontSendNotification);
        copyrightLabel.setFont(ModelStyle::font(12.0f));
        copyrightLabel.setColour(Label::textColourId, ModelStyle::secondaryText);
        addAndMakeVisible(copyrightLabel);

        for (auto* label : { &nameLabel,
                             &versionLabel,
                             &descriptionLabel,
                             &acknowledgementsLabel,
                             &copyrightLabel })
        {
            label->setJustificationType(Justification::centredLeft);
            label->setBorderSize({ 0, 0, 0, 0 });
        }

        setSize(windowWidth, windowHeight);
    }

    void paint(Graphics& g) override
    {
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized() override
    {
        FlexBox column;
        column.flexDirection = FlexBox::Direction::column;

        auto addRow = [&column](Component& component, float height, float marginAbove = 0.0f)
        {
            column.items.add(
                FlexItem(component).withHeight(height).withMargin({ marginAbove, 0, 0, 0 }));
        };

        addRow(nameLabel, 30);
        addRow(versionLabel, rowHeight);
        addRow(descriptionLabel, 2 * rowHeight, sectionGap / 2);

        addRow(linksHeader, ModelStyle::sectionHeaderHeight, sectionGap);
        addRow(websiteLink, rowHeight);
        addRow(modelGlossaryLink, rowHeight);
        addRow(reportIssueLink, rowHeight);

        addRow(acknowledgementsHeader, ModelStyle::sectionHeaderHeight, sectionGap);
        addRow(acknowledgementsLabel, rowHeight);
        addRow(noticesLink, rowHeight);

        // Whatever height is left over sits above the copyright notice
        column.items.add(FlexItem().withFlex(1));
        addRow(copyrightLabel, rowHeight);

        column.performLayout(getLocalBounds().reduced(pagePadding));
    }

private:
    void addLink(HyperlinkButton& link, const String& text, const String& url)
    {
        link.setButtonText(text);
        link.setURL(URL(url));
        link.setFont(ModelStyle::font(14.0f), false, Justification::centredLeft);
        link.setColour(HyperlinkButton::textColourId, ModelStyle::accent);
        addAndMakeVisible(link);
    }

    static constexpr int windowWidth = 400;
    static constexpr int windowHeight = 360;
    // Space around the contents, as around a model tab
    static constexpr int pagePadding = 14;
    static constexpr float rowHeight = 22;
    static constexpr float sectionGap = 12;

    Label nameLabel;
    Label versionLabel;
    Label descriptionLabel;

    ModelStyle::SectionHeader linksHeader { "Links" };
    HyperlinkButton websiteLink;
    HyperlinkButton modelGlossaryLink;
    HyperlinkButton reportIssueLink;

    ModelStyle::SectionHeader acknowledgementsHeader { "Acknowledgements" };
    Label acknowledgementsLabel;
    HyperlinkButton noticesLink;

    Label copyrightLabel;
};
