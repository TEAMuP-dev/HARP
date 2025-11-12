#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include "../AppSettings.h"

using namespace juce;

class WelcomeWindow : public Component
{
public:
    WelcomeWindow(std::function<void()> openSettingsCallback)
        : onOpenSettings(std::move(openSettingsCallback))
    {
        setSize(480, 500);
        setColour(ResizableWindow::backgroundColourId, Colours::darkgrey);

        // --- Intro Text ---
        introText.setText(
            "Welcome to HARP!\n"
            "A tool for hosted, asynchronous processing of audio tracks.",
            dontSendNotification);
        introText.setJustificationType(Justification::centred);
        introText.setFont(Font(17.0f, Font::bold));
        introText.setBounds(20, 15, 440, 50);
        addAndMakeVisible(introText);

        // --- Instructions ---
        instructions.setText(
            "\n HARP operates as a standalone app or plugin-like editor within your DAW, allowing you "
            "to process tracks using ML models hosted on platforms like Hugging Face or Stability AI.\n\n"
            "The interface is organized into three main sections:\n"
            "1. The top area allows model selection and parameter control.\n"
            "2. The middle area handles audio & MIDI input and processing.\n"
            "3. The bottom shows model status & info for hovered component.\n\n"
            "GET STARTED:\n\n"
            "Access tokens are required for models hosted on Hugging Face or Stability AI, "
            "to authenticate your account and let HARP securely fetch and run models. "
            "The first two models in the dropdown are on Stability, and the rest are on Hugging Face.\n\n"
            "Add tokens under File -> Settings -> Hugging Face, or by clicking 'Open Settings' below.",
            dontSendNotification);
        instructions.setJustificationType(Justification::centredTop);
        instructions.setFont(Font(14.0f));
        instructions.setBounds(30, 70, 420, 270);
        addAndMakeVisible(instructions);

        // --- Buttons and Layout Adjustments ---
        const int buttonWidth = 200;
        const int buttonX = (getWidth() - buttonWidth) / 2; // center align

        openSettingsButton.setButtonText("Open Settings");
        openSettingsButton.setSize(buttonWidth, 30);
        openSettingsButton.setTopLeftPosition(buttonX, 355);
        openSettingsButton.onClick = [this]()
        {
            if (onOpenSettings)
                onOpenSettings();
        };
        addAndMakeVisible(openSettingsButton);

        // --- Centered checkbox ---
        dontShowAgain.setButtonText("Don't show this again");
        dontShowAgain.setSize(buttonWidth, 24);
        dontShowAgain.setTopLeftPosition(buttonX, 390);
        dontShowAgain.setColour(ToggleButton::textColourId, Colours::white);
        addAndMakeVisible(dontShowAgain);

        continueButton.setButtonText("Continue");
        continueButton.setSize(buttonWidth, 30);
        continueButton.setTopLeftPosition(buttonX, 420);
        continueButton.onClick = [this]()
        {
            const bool dontShow = dontShowAgain.getToggleState();
            AppSettings::setValue("showWelcomePopup", dontShow ? 0 : 1);
            AppSettings::saveIfNeeded();

            if (auto* window = findParentComponentOfClass<DialogWindow>())
                window->closeButtonPressed();
        };
        addAndMakeVisible(continueButton);

        // --- View Documentation ---
        docsLink.setButtonText("View Documentation");
        docsLink.setURL(URL("https://harp-plugin.netlify.app/content/intro.html"));
        docsLink.setColour(HyperlinkButton::textColourId, Colours::skyblue);
        docsLink.setSize(buttonWidth, 20);
        docsLink.setTopLeftPosition(buttonX, 455);
        addAndMakeVisible(docsLink);

        // --- Footer ---
        footerLabel.setText("Copyright 2025 TEAMuP. All rights reserved.",
                            dontSendNotification);
        footerLabel.setJustificationType(Justification::centred);
        footerLabel.setFont(Font(13.0f));
        footerLabel.setBounds(0, 475, 480, 20);
        addAndMakeVisible(footerLabel);
    }

    void paint(Graphics& g) override
    {
        g.fillAll(findColour(ResizableWindow::backgroundColourId));
    }

private:
    std::function<void()> onOpenSettings;
    Label introText;
    Label instructions;
    TextButton openSettingsButton;
    ToggleButton dontShowAgain;
    TextButton continueButton;
    HyperlinkButton docsLink;
    Label footerLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WelcomeWindow)
};
