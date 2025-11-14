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
        addAndMakeVisible(instructions);

        // --- Buttons ---
        openSettingsButton.setButtonText("Open Settings");
        openSettingsButton.onClick = [this]()
        {
            if (onOpenSettings)
                onOpenSettings();
        };
        addAndMakeVisible(openSettingsButton);

        // --- Checkbox ---
        dontShowAgain.setButtonText("Don't show this again");
        dontShowAgain.setColour(ToggleButton::textColourId, Colours::white);
        addAndMakeVisible(dontShowAgain);

        continueButton.setButtonText("Continue");
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
        addAndMakeVisible(docsLink);

        // --- Footer ---
        footerLabel.setText("Copyright 2025 TEAMuP. All rights reserved.",
                            dontSendNotification);
        footerLabel.setJustificationType(Justification::centred);
        footerLabel.setFont(Font(13.0f));
        addAndMakeVisible(footerLabel);
    }

    void paint(Graphics& g) override
    {
        g.fillAll(findColour(ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        // Get the content area dimensions
        auto area = getLocalBounds();
        
        // Define content width (same as original 480 width minus padding)
        const int contentWidth = 440;
        const int buttonWidth = 200;
        
        // Calculate horizontal center offset
        const int centerX = (area.getWidth() - contentWidth) / 2;
        const int buttonX = (area.getWidth() - buttonWidth) / 2;
        
        // Calculate vertical center offset for the entire content block
        const int totalContentHeight = 485; // Approximate total height of all content
        const int startY = jmax(0, (area.getHeight() - totalContentHeight) / 2);
        
        // Position all elements relative to center
        introText.setBounds(centerX, startY + 15, contentWidth, 50);
        instructions.setBounds(centerX + 10, startY + 70, contentWidth - 20, 270);
        openSettingsButton.setBounds(buttonX, startY + 355, buttonWidth, 30);
        dontShowAgain.setBounds(buttonX, startY + 390, buttonWidth, 24);
        continueButton.setBounds(buttonX, startY + 420, buttonWidth, 30);
        docsLink.setBounds(buttonX, startY + 455, buttonWidth, 20);
        footerLabel.setBounds(centerX - 20, startY + 475, contentWidth + 40, 20);
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