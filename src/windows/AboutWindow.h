#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

using namespace juce;

class AboutWindow : public Component
{
public:
    AboutWindow()
    {
        setSize(400, 300);

        // label for the about text
        aboutText.setText(String(APP_NAME) + "\nVersion: " + String(APP_VERSION) + "\n\n",
                          dontSendNotification);
        aboutText.setJustificationType(Justification::centred);
        aboutText.setSize(380, 100);
        addAndMakeVisible(aboutText);

        // hyperlink buttons
        modelGlossaryButton.setButtonText("Model Glossary");
        modelGlossaryButton.setURL(URL("https://harp-plugin.netlify.app/content/usage/models.html"));
        modelGlossaryButton.setSize(380, 24);
        modelGlossaryButton.setTopLeftPosition(10, 110);
        modelGlossaryButton.setColour(HyperlinkButton::textColourId, Colours::blue);
        addAndMakeVisible(modelGlossaryButton);

        visitWebpageButton.setButtonText("Visit HARP webpage");
        visitWebpageButton.setURL(URL("https://harp-plugin.netlify.app/"));
        visitWebpageButton.setSize(380, 24);
        visitWebpageButton.setTopLeftPosition(10, 140);
        visitWebpageButton.setColour(HyperlinkButton::textColourId, Colours::blue);
        addAndMakeVisible(visitWebpageButton);

        reportIssueButton.setButtonText("Report an issue");
        reportIssueButton.setURL(URL("https://github.com/TEAMuP-dev/HARP/issues"));
        reportIssueButton.setSize(380, 24);
        reportIssueButton.setTopLeftPosition(10, 170);
        reportIssueButton.setColour(HyperlinkButton::textColourId, Colours::blue);
        addAndMakeVisible(reportIssueButton);

        // label for the copyright
        copyrightLabel.setText(String(APP_COPYRIGHT) + "\n\n", dontSendNotification);
        copyrightLabel.setJustificationType(Justification::centred);
        copyrightLabel.setSize(380, 100);
        copyrightLabel.setTopLeftPosition(10, 200);
        addAndMakeVisible(copyrightLabel);
    }

private:
    Label aboutText;
    HyperlinkButton modelGlossaryButton;
    HyperlinkButton visitWebpageButton;
    HyperlinkButton reportIssueButton;
    Label copyrightLabel;
};
