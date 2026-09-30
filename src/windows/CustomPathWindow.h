/**
 * @file CustomPathWindow.h
 * @brief Popup for entering the path of a model that is not listed on the Home tab.
 * @author cwitkowitz, xribene
 */

#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../utils/Clients.h"
#include "../utils/Interface.h"
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
        pathEditor.setTextToShowWhenEmpty("e.g., user/space-name or a Gradio URL", Colours::grey);
        pathEditor.onTextChange = [this]
        {
            loadButton.setEnabled(! pathEditor.isEmpty());
            errorLabel.setText({}, dontSendNotification);
        };
        pathEditor.onReturnKey = [this]
        {
            if (loadButton.isEnabled())
            {
                loadButton.triggerClick();
            }
        };
        addAndMakeVisible(pathEditor);

        errorLabel.setColour(Label::textColourId, Colours::orange);
        errorLabel.setJustificationType(Justification::centredLeft);
        addAndMakeVisible(errorLabel);

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
                    if (safeThis != nullptr)
                        safeThis->pathEditor.grabKeyboardFocus();
                });
        }
    }

    void resized() override
    {
        Rectangle<int> fullArea = getLocalBounds();

        /* TopLevelWindow::centreAroundComponent shrinks a dialog to fit the area it
           is centered within, so this can be laid out smaller than the size asked
           for. Fixed items would then exceed the space and leave the rest negative. */
        if (fullArea.isEmpty())
        {
            for (auto* child : getChildren())
                child->setBounds({});

            return;
        }

        const int editorHeight = jmin(editorRowHeight, fullArea.getHeight());

        FlexBox fullPopup;
        fullPopup.flexDirection = FlexBox::Direction::column;

        fullPopup.items.add(FlexItem(pathEditor)
                                .withHeight((float) editorHeight)
                                .withMargin(jmin(2.0f, (float) fullArea.getHeight() / 4.0f)));

        fullPopup.items.add(
            FlexItem(errorLabel).withHeight((float) errorRowHeight).withMinHeight(0.0f));

        FlexBox buttonsArea;
        buttonsArea.flexDirection = FlexBox::Direction::row;

        // Margins have to shrink with the popup, or they consume more than there is
        const float buttonMargin =
            jmin(10.0f, (float) jmin(fullArea.getWidth(), fullArea.getHeight()) / 8.0f);

        buttonsArea.items.add(FlexItem(loadButton).withFlex(1).withMargin(buttonMargin));
        buttonsArea.items.add(FlexItem().withFlex(0.25));
        buttonsArea.items.add(FlexItem(cancelButton).withFlex(1).withMargin(buttonMargin));

        fullPopup.items.add(FlexItem(buttonsArea).withFlex(1).withMinHeight(0.0f));

        fullPopup.performLayout(fullArea);

        // FlexBox can still hand out negative sizes when the space runs out
        for (auto* child : getChildren())
        {
            if (child->getWidth() < 0 || child->getHeight() < 0)
            {
                child->setBounds(child->getBounds().withSize(jmax(0, child->getWidth()),
                                                             jmax(0, child->getHeight())));
            }
        }
    }

    void paint(Graphics& g) override
    {
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    /**
     * Launches the popup centered around a component. The path entered is passed to
     * onLoad, reduced to the form its provider recognizes.
     */
    static void launch(Component* componentToCentreAround, std::function<void(String)> onLoad)
    {
        DialogWindow::LaunchOptions options;
        options.dialogTitle = "Enter Custom Path";
        options.dialogBackgroundColour = Colours::darkgrey;
        options.content.setOwned(new CustomPathComponent(std::move(onLoad)));

        options.useNativeTitleBar = false;
        options.resizable = false;
        options.escapeKeyTriggersCloseButton = true;
        options.componentToCentreAround = componentToCentreAround;

        options.launchAsync();
    }

private:
    void submit()
    {
        const String path = canonicalizeModelPath(pathEditor.getText().trim());

        // Caught here rather than by opening a tab that could only fail
        if (! isSupportedModelPath(path))
        {
            errorLabel.setText("No supported provider recognizes this path.", dontSendNotification);

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
        if (auto* popup = findParentComponentOfClass<DialogWindow>())
        {
            popup->exitModalState(0);
        }
    }

    static constexpr int popupWidth = 400;
    static constexpr int popupHeight = 100;
    static constexpr int editorRowHeight = 30;
    static constexpr int errorRowHeight = 20;

    TextEditor pathEditor;
    Label errorLabel;
    TextButton loadButton { "Load" };
    TextButton cancelButton { "Cancel" };

    std::function<void(String)> onLoadCallback;
};
