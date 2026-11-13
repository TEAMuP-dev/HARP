/**
 * @file CustomPathWindow.h
 * @brief Popup for entering the path of a model that is not listed on the Home tab.
 * @author cwitkowitz, xribene
 */

#pragma once

#include <functional>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../gui/Palette.h"
#include "../gui/PopupPanel.h"

#include "../utils/Clients.h"
#include "../utils/Logging.h"

using namespace juce;

class CustomPathComponent : public Component
{
public:
    explicit CustomPathComponent(std::function<void(String)> onLoad)
        : onLoadCallback(std::move(onLoad))
    {
        pathEditor.setMultiLine(false);
        pathEditor.setReturnKeyStartsNewLine(false);
        pathEditor.onTextChange = [this]
        {
            loadButton.setEnabled(! pathEditor.isEmpty());
            showHint();
        };
        pathEditor.onReturnKey = [this]
        {
            if (loadButton.isEnabled())
            {
                loadButton.triggerClick();
            }
        };
        // The editor takes the key itself, which would otherwise close the popup
        pathEditor.onEscapeKey = [this] { closePopup(); };
        addAndMakeVisible(pathEditor);

        /* Says what to enter until there is something wrong with what was entered. The editor
           has the keyboard from the start, so text shown inside it while empty would never be
           seen. */
        noteLabel.setFont(ModelStyle::font(13.0f));
        noteLabel.setJustificationType(Justification::centredLeft);
        noteLabel.setMinimumHorizontalScale(1.0f);
        addAndMakeVisible(noteLabel);
        showHint();

        loadButton.setEnabled(false);
        loadButton.onClick = [this] { submit(); };
        addAndMakeVisible(loadButton);

        cancelButton.onClick = [this] { closePopup(); };
        addAndMakeVisible(cancelButton);

        setSize(popupWidth, popupHeight);
    }

    void visibilityChanged() override
    {
        if (isVisible())
        {
            SafePointer<CustomPathComponent> safeThis(this);

            MessageManager::callAsync(
                [safeThis]
                {
                    if (safeThis != nullptr && safeThis->isShowing())
                        safeThis->pathEditor.grabKeyboardFocus();
                });
        }
    }

    void resized() override
    {
        FlexBox fullPopup;
        fullPopup.flexDirection = FlexBox::Direction::column;

        fullPopup.items.add(FlexItem(pathEditor).withHeight(editorRowHeight));
        fullPopup.items.add(
            FlexItem(noteLabel).withHeight(noteRowHeight).withMargin({ 4, 0, 0, 0 }));

        // Whatever height is left over sits between the note and the buttons
        fullPopup.items.add(FlexItem().withFlex(1));

        FlexBox buttonsArea;
        buttonsArea.flexDirection = FlexBox::Direction::row;
        buttonsArea.justifyContent = FlexBox::JustifyContent::flexEnd;

        buttonsArea.items.add(FlexItem(cancelButton).withWidth(buttonWidth));
        buttonsArea.items.add(
            FlexItem(loadButton).withWidth(buttonWidth).withMargin({ 0, 0, 0, buttonGap }));

        fullPopup.items.add(FlexItem(buttonsArea).withHeight(buttonRowHeight));

        fullPopup.performLayout(getLocalBounds());
    }

    /**
     * Creates the popup, for whatever shows it to own. The path entered is passed to onLoad,
     * reduced to the form its provider recognizes.
     */
    static std::unique_ptr<PopupPanel> createPopup(std::function<void(String)> onLoad)
    {
        return std::make_unique<PopupPanel>(
            "Enter Custom Path", std::make_unique<CustomPathComponent>(std::move(onLoad)));
    }

private:
    void showHint()
    {
        noteLabel.setColour(Label::textColourId, ModelStyle::secondaryText);
        noteLabel.setText("A Hugging Face Space (user/space-name), Gradio URL, or local address.",
                          dontSendNotification);
    }

    void showProblem(const String& problem)
    {
        noteLabel.setColour(Label::textColourId, ModelStyle::problem);
        noteLabel.setText(problem, dontSendNotification);
    }

    void submit()
    {
        const String path = canonicalizeModelPath(pathEditor.getText().trim());

        // Caught here rather than by opening a tab that could only fail
        if (! isSupportedModelPath(path))
        {
            showProblem("No supported provider recognizes this path.");

            return;
        }

        DBG_AND_LOG("CustomPathComponent::submit: Custom path \"" << path << "\" entered.");

        if (onLoadCallback)
        {
            onLoadCallback(path);
        }

        closePopup();
    }

    void closePopup()
    {
        if (auto* popup = findParentComponentOfClass<PopupPanel>())
        {
            popup->dismiss();
        }
    }

    static constexpr int popupWidth = 440;
    static constexpr int popupHeight = 96;
    static constexpr float editorRowHeight = 28;
    static constexpr float noteRowHeight = 20;
    static constexpr float buttonRowHeight = 26;
    static constexpr float buttonWidth = 90;
    static constexpr float buttonGap = 8;

    TextEditor pathEditor;
    Label noteLabel;
    TextButton loadButton { "Load" };
    TextButton cancelButton { "Cancel" };

    std::function<void(String)> onLoadCallback;
};
