/**
 * @file CustomPathDialog.cpp
 * @brief Custom dialog for entering a custom path in the HARPPlugin
 * @author xribene
 */

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include <functional>

inline Colour getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour uiColour,
                                     Colour fallback = Colour(0xff4d4d4d)) noexcept
{
    if (auto* v4 = dynamic_cast<LookAndFeel_V4*>(&LookAndFeel::getDefaultLookAndFeel()))
        return v4->getCurrentColourScheme().getUIColour(uiColour);

    return fallback;
}

class CustomPathComponent : public juce::Component
{
public:
    CustomPathComponent(std::function<void(const juce::String&)> onLoadCallback,
                        std::function<void()> onCancelCallback)
        : m_onLoadCallback(onLoadCallback)
    {
        // Set up the TextEditor for path input
        addAndMakeVisible(customPathEditor);
        customPathEditor.setMultiLine(false);
        customPathEditor.setReturnKeyStartsNewLine(false);
        customPathEditor.onTextChange = [this]()
        { loadButton.setEnabled(customPathEditor.getText().isNotEmpty()); };

        // Set up the Load button
        addAndMakeVisible(loadButton);
        loadButton.setButtonText("Load");
        loadButton.setEnabled(false); // Initially disabled
        loadButton.onClick = [this, onLoadCallback]()
        {
            if (onLoadCallback)
            {
                onLoadCallback(customPathEditor.getText());
                closeDialog();
            }
        };

        // Set up the Cancel button
        addAndMakeVisible(cancelButton);
        cancelButton.setButtonText("Cancel");
        cancelButton.onClick = [this, onCancelCallback]()
        {
            if (onCancelCallback)
            {
                onCancelCallback();
            }
            closeDialog();
        };

        setSize(400, 150); // Set a fixed size for the component
    }

    void closeDialog()
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        {
            dw->closeButtonPressed(); // Close the dialog
        }
    }

    void visibilityChanged() override
    {
        if (isVisible())
        {
            MessageManager::callAsync([this] { customPathEditor.grabKeyboardFocus(); });
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        customPathEditor.setBounds(area.removeFromTop(40));
        // button area
        auto buttonArea = area.removeFromBottom(40);
        loadButton.setBounds(buttonArea.removeFromLeft(buttonArea.getWidth() / 2));
        cancelButton.setBounds(buttonArea);
    }

    void paint(Graphics& g) override
    {
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

private:
    juce::TextEditor customPathEditor;
    juce::TextButton loadButton, cancelButton;
    std::function<void(const juce::String&)> m_onLoadCallback;
};

class CustomPathDialog
{
public:
    static void showDialogWindow(std::function<void(const juce::String&)> onLoadCallback,
                                 std::function<void()> onCancelCallback)
    {
        // Create the custom path component
        auto* customPathComponent = new CustomPathComponent(onLoadCallback, onCancelCallback);

        // Set up the launch options
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(
            customPathComponent); // Transfer ownership of the component to JUCE

        // Set size of the content component
        options.content->setSize(400, 150);

        // Dialog window options
        options.dialogTitle = "Enter Custom Path";
        options.dialogBackgroundColour = juce::Colours::lightgrey;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = false;
        options.resizable = false;

        // Launch the dialog asynchronously
        auto* dialogWindow = options.launchAsync();

        // Optionally center the dialog window
        if (dialogWindow != nullptr)
            dialogWindow->centreWithSize(400, 150);
    }
};