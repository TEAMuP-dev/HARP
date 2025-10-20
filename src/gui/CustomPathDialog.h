/**
 * @file CustomPathDialog.cpp
 * @brief Custom dialog for entering a custom path in the HARPPlugin
 * @author xribene
 */

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include <functional>
#include "../utils.h"

class CustomPathComponent : public Component, public juce::TextEditor::Listener
{
public:
    CustomPathComponent(std::function<void(const String&)> onLoadCallback,
                        std::function<void()> onCancelCallback)
    {
        // Set up the TextEditor for path input
        addAndMakeVisible(customPathEditor);
        customPathEditor.setMultiLine(false);
        customPathEditor.setReturnKeyStartsNewLine(false);
        customPathEditor.onTextChange = [this]()
        { loadButton.setEnabled(customPathEditor.getText().isNotEmpty()); };

        customPathEditor.addListener(this); //listen for the enter key press

        // Set up the Load button
        addAndMakeVisible(loadButton);
        loadButton.setButtonText("Load");
        loadButton.setEnabled(false); // Initially disabled
        loadButton.onClick = [this, onLoadCallback]()
        { onLoadCallback(customPathEditor.getText()); };

        // Set up the Cancel button
        addAndMakeVisible(cancelButton);
        cancelButton.setButtonText("Cancel");
        cancelButton.onClick = [onCancelCallback]() { onCancelCallback(); };

        setSize(400, 150); // Set a fixed size for the component
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

    void textEditorReturnKeyPressed(juce::TextEditor&) override
    {
        if (loadButton.isEnabled())
            loadButton.triggerClick();
    }

    void setTextFieldValue(const std::string& path)
    {
        customPathEditor.setText(path, juce::dontSendNotification);
        customPathEditor.selectAll();
    }

private:
    TextEditor customPathEditor;
    TextButton loadButton, cancelButton;
    CustomPathComponent* pathComponent = nullptr;
};

class CustomPathDialog : public DialogWindow
{
public:
    CustomPathDialog(std::function<void(const String&)> onLoadCallback,
                     std::function<void()> onCancelCallback)
        : DialogWindow("Enter Custom Path", Colours::lightgrey, true),
          m_onLoadCallback(onLoadCallback),
          m_onCancelCallback(onCancelCallback)
    {
        // Create the content component
        //auto* content =
        // new CustomPathComponent([this](const String& path) { loadButtonPressed(path); },
        //[this]() { cancelButtonPressed(); });
        pathComponent =
            new CustomPathComponent([this](const String& path) { loadButtonPressed(path); },
                                    [this]() { cancelButtonPressed(); });

        setContentOwned(pathComponent, true);

        // Add custom content
        //setContentOwned(content, true);

        // Other dialog window options
        setUsingNativeTitleBar(false);
        setResizable(false, false);

        // Set size of the content component and center
        centreWithSize(400, 150);
        setVisible(true);

        // Ensure user cannot click off this window
        DialogWindow::enterModalState(true, nullptr, true);
    }

    void loadButtonPressed(const String& path)
    {
        if (m_onLoadCallback)
        {
            m_onLoadCallback(path);

            closeWindow();
        }
    }

    void cancelButtonPressed() { closeButtonPressed(); }

    void closeButtonPressed() override
    {
        if (m_onCancelCallback)
        {
            m_onCancelCallback();
        }

        closeWindow();
    }

    void closeWindow()
    {
        setVisible(false);
        delete this;
    }

    void setTextFieldValue(const std::string& path)
    {
        if (pathComponent != nullptr)
            pathComponent->setTextFieldValue(path);
    }

private:
    std::function<void(const String&)> m_onLoadCallback;
    std::function<void()> m_onCancelCallback;
    CustomPathComponent* pathComponent = nullptr;
};
