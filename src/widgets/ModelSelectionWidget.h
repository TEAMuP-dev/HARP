/**
 * @file ModelSelectionWidget.h
 * @brief Component allowing for selection and loading of model.
 * @author hugofloresgarcia, rc2000123, xribene, lindseydeng, cwitkowitz
 */

#pragma once

#include <any>
#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../widgets/StatusAreaWidget.h"

#include "../gui/HoverHandler.h"
#include "../gui/MultiButton.h"

#include "../utils/Clients.h"
#include "../utils/Errors.h"
#include "../utils/Interface.h"
#include "../utils/Logging.h"

using namespace juce;

struct SharedChoices : public ChangeBroadcaster
{
    int getIndexForPath(const std::string& p)
    {
        int idx = -1;

        for (unsigned int i = 0; i < savedModelPaths.size(); ++i)
        {
            if (savedModelPaths[i] == p)
            {
                idx = (int) i;

                break;
            }
        }

        return idx;
    }

    // TODO - should check endpoint path - otherwise could be duplicates
    bool containsPath(const std::string& p) { return getIndexForPath(p) != -1; }

    void addNewPath(const std::string& p)
    {
        savedModelPaths.push_back(p);
        sendSynchronousChangeMessage();
    }

    void updatePath(unsigned int idx, const std::string& p)
    {
        savedModelPaths[idx] = p;
        sendSynchronousChangeMessage();
    }

    std::vector<std::string> savedModelPaths = {
        "click here to enter a custom path...",
        "stability/text-to-audio",
        "stability/audio-to-audio",
        "teamup-tech/text2midi-symbolic-music-generation",
        "teamup-tech/demucs-source-separation",
        "teamup-tech/solo-piano-audio-to-midi-transcription",
        "teamup-tech/transkun", // TODO - more intuitive name
        "teamup-tech/TRIA", // TODO - more intuitive name: (The Rhythm In Anything) conditional drum generation
        "teamup-tech/anticipatory-music-transformer",
        "teamup-tech/vampnet-conditional-music-generation",
        "teamup-tech/harmonic-percussive-separation",
        "teamup-tech/Kokoro-TTS",
        "teamup-tech/MegaTTS3-Voice-Cloning",
        "teamup-tech/midi-synthesizer",
        "teamup-tech/audioseal", // TODO - more intuitive name
        // "xribene/HARP-UI-TEST-v3"
    };
};

class CustomPathComponent : public Component
{
public:
    CustomPathComponent(std::function<void(String)> onLoad, std::function<void()> onCancel)
        : onLoadCallback(std::move(onLoad)), onCancelCallback(std::move(onCancel))
    {
        pathEditor.setMultiLine(false);
        pathEditor.setReturnKeyStartsNewLine(false);
        pathEditor.onTextChange = [this] { loadButton.setEnabled(! pathEditor.isEmpty()); };
        pathEditor.onReturnKey = [this]
        {
            if (loadButton.isEnabled())
            {
                loadButton.triggerClick();
            }
        };
        addAndMakeVisible(pathEditor);

        loadButton.setEnabled(false);
        loadButton.onClick = [this]
        {
            wasLoadPressed = true;

            if (onLoadCallback)
            {
                onLoadCallback(pathEditor.getText());
            }

            closePopup();
        };
        addAndMakeVisible(loadButton);

        cancelButton.onClick = [this] { closePopup(); };
        addAndMakeVisible(cancelButton);

        setSize(popupWidth, popupHeight);
    }

    ~CustomPathComponent() override
    {
        if (! wasLoadPressed && onCancelCallback)
        {
            // Treat as cancel if closed without load
            onCancelCallback();
        }
    }

    void visibilityChanged() override
    {
        if (isVisible())
        {
            MessageManager::callAsync([this] { pathEditor.grabKeyboardFocus(); });
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
            pathEditor.setBounds({});
            loadButton.setBounds({});
            cancelButton.setBounds({});

            return;
        }

        const int editorHeight = jmin(editorRowHeight, fullArea.getHeight());

        FlexBox fullPopup;
        fullPopup.flexDirection = FlexBox::Direction::column;

        fullPopup.items.add(FlexItem(pathEditor)
                                .withHeight((float) editorHeight)
                                .withMargin(jmin(2.0f, (float) fullArea.getHeight() / 4.0f)));

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

    void setTextFieldValue(const String& text)
    {
        pathEditor.setText(text, dontSendNotification);
        pathEditor.selectAll();
    }

private:
    void closePopup()
    {
        if (auto* popup = findParentComponentOfClass<DialogWindow>())
        {
            popup->exitModalState(0);
        }
    }

    static constexpr int popupWidth = 400;
    static constexpr int popupHeight = 80;
    static constexpr int editorRowHeight = 30;

    TextEditor pathEditor;
    TextButton loadButton { "Load" };
    TextButton cancelButton { "Cancel" };

    bool wasLoadPressed = false;

    std::function<void(String)> onLoadCallback;
    std::function<void()> onCancelCallback;
};

class ModelSelectionWidget : public Component, public ChangeBroadcaster, public ChangeListener
{
public:
    ModelSelectionWidget()
    {
        initializeLoadModelButton();
        initializeModelPathComboBox();

        resetState();

        sharedChoices->addChangeListener(this);
    }

    ~ModelSelectionWidget() { sharedChoices->removeChangeListener(this); }

    void resized() override
    {
        FlexBox selectionArea;
        selectionArea.flexDirection = FlexBox::Direction::row;

        selectionArea.items.add(FlexItem(modelPathComboBox).withFlex(1).withMargin(marginSize));
        selectionArea.items.add(FlexItem(loadModelButton).withWidth(100).withMargin(marginSize));

        selectionArea.performLayout(getLocalBounds());
    }

    String getCurrentlySelectedPath() { return selectedPath; }

    void loadModelBypass(const String& modelPath)
    {
        /* Reduce to the canonical form on the way in, so that the same model
           entered three different ways is loaded, listed, and matched as one
           entry rather than accumulating duplicates in the dropdown. */
        selectedPath = canonicalizeModelPath(modelPath);

        if (selectedPath != modelPath)
        {
            DBG_AND_LOG("ModelSelectionWidget::loadModelBypass: Path \""
                        << modelPath << "\" resolved to \"" << selectedPath << "\".");
        }

        sendChangeMessage();
    }

    void resetState()
    {
        lastLoadedPathIndex = -1;
        lastSelectedPathIndex = -1;
        modelPathComboBox.setSelectedId(lastSelectedPathIndex);
        modelPathComboBox.setEnabled(true);

        loadModelButton.setEnabled(false);
    }

    void setDisabled()
    {
        modelPathComboBox.setEnabled(false);
        loadModelButton.setEnabled(false);
    }

    void setEnabled()
    {
        modelPathComboBox.setEnabled(true);
        loadModelButton.setEnabled(true);
    }

    void setFinishedState()
    {
        setEnabled();

        modelPathComboBox.setSelectedId(lastLoadedPathIndex + 1);
    }

    /* The path a model actually loaded from can differ from the one entered, since
       the provider is asked for its exact spelling. The dropdown lists that one, so
       the three ways of writing an address collapse to a single entry. */
    void setSuccessfulState(const String& resolvedPath)
    {
        if (resolvedPath.isNotEmpty())
        {
            selectedPath = resolvedPath;
        }

        std::string loadedPath = selectedPath.toStdString();

        if (! sharedChoices->containsPath(loadedPath))
        {
            if (sharedChoices->containsPath(loadedPath + validPathBrokenTag))
            {
                unsigned int currentIdx =
                    (unsigned int) sharedChoices->getIndexForPath(loadedPath + validPathBrokenTag);

                // Remove broken tag from existing entry for path
                sharedChoices->updatePath(currentIdx, loadedPath);
            }
            else if (sharedChoices->containsPath(loadedPath + validPathTryAgainTag))
            {
                unsigned int currentIdx = (unsigned int) sharedChoices->getIndexForPath(
                    loadedPath + validPathTryAgainTag);

                // Remove try again tag from existing entry for path
                sharedChoices->updatePath(currentIdx, loadedPath);
            }
            else if (sharedChoices->containsPath(loadedPath + validPathErrorTag))
            {
                unsigned int currentIdx =
                    (unsigned int) sharedChoices->getIndexForPath(loadedPath + validPathErrorTag);

                // Remove error tag from existing entry for path
                sharedChoices->updatePath(currentIdx, loadedPath);
            }
            else
            {
                // Add a new entry for custom path
                sharedChoices->addNewPath(loadedPath);

                lastSelectedPathIndex = sharedChoices->getIndexForPath(loadedPath);
            }
        }

        lastLoadedPathIndex = sharedChoices->getIndexForPath(loadedPath);

        setFinishedState();
    }

    void setUnsuccessfulState(const Error& error)
    {
        bool wasValidPath = true;

        if (const auto* e = std::get_if<ClientError>(&error))
        {
            wasValidPath = false;
        }

        if (const auto* e = std::get_if<HttpError>(&error))
        {
            if (e->type == HttpError::Type::BadStatusCode && e->statusCode == 404)
            {
                wasValidPath = false;
            }
        }

        if (! wasValidPath)
        {
            if (modelPathComboBox.getSelectedItemIndex() == 0)
            {
                openCustomPathPopup(selectedPath);

                return;
            }
        }

        std::string originalEntry = selectedPath.toStdString();
        std::string updatedEntry = selectedPath.toStdString();

        if (const auto* e = std::get_if<HttpError>(&error))
        {
            if (e->type == HttpError::Type::ConnectionFailed
                && e->request == HttpError::Request::POST)
            {
                updatedEntry += validPathTryAgainTag;
            }
            else if (e->type == HttpError::Type::BadStatusCode && e->statusCode == 429)
            {
                // Rate limiting says nothing about the model, only about how
                // quickly it was asked for
                updatedEntry += validPathTryAgainTag;
            }
            else if (e->type == HttpError::Type::BadStatusCode && e->statusCode == 503)
            {
                updatedEntry += validPathBrokenTag;
            }
            else
            {
                updatedEntry += validPathErrorTag;
            }
        }
        else if (const auto* e = std::get_if<GradioError>(&error))
        {
            /* A Space that is waking up is not a broken one, so it must not be
               marked as down. The Hub was queried to determine the stage. */
            if (e->type == GradioError::Type::SpaceStarting)
            {
                updatedEntry += validPathTryAgainTag;
            }
            else if (e->type == GradioError::Type::SpaceUnavailable)
            {
                updatedEntry += validPathBrokenTag;
            }
            else
            {
                updatedEntry += validPathErrorTag;
            }
        }
        else
        {
            updatedEntry += validPathErrorTag;
        }

        // Check for previously added unsuccessful tags before querying
        if (sharedChoices->containsPath(originalEntry + validPathErrorTag))
        {
            originalEntry += validPathErrorTag;
        }
        if (sharedChoices->containsPath(originalEntry + validPathBrokenTag))
        {
            originalEntry += validPathBrokenTag;
        }
        if (sharedChoices->containsPath(originalEntry + validPathTryAgainTag))
        {
            originalEntry += validPathTryAgainTag;
        }

        if (sharedChoices->containsPath(updatedEntry))
        {
            // Path has already been updated
        }
        else if (sharedChoices->containsPath(originalEntry))
        {
            unsigned int currentIdx = (unsigned int) sharedChoices->getIndexForPath(originalEntry);

            // Update entry with tag for existing path
            sharedChoices->updatePath(currentIdx, updatedEntry);
        }
        else
        {
            // Add a new entry with tag for custom path
            sharedChoices->addNewPath(updatedEntry);
        }

        lastSelectedPathIndex = lastLoadedPathIndex;

        selectedPath.clear();

        setFinishedState();
    }

    void changeListenerCallback(ChangeBroadcaster* /*source*/) { resetModelPathComboBox(); }

private:
    void resetModelPathComboBox()
    {
        modelPathComboBox.clear();

        for (unsigned int i = 0; i < sharedChoices->savedModelPaths.size(); ++i)
        {
            // Add saved path to combo box (skipping 0 for custom path)
            modelPathComboBox.addItem(sharedChoices->savedModelPaths[i], static_cast<int>(i) + 1);
        }
    }

    void initializeModelPathComboBox()
    {
        modelPathComboBox.setTextWhenNothingSelected("click here to select a model...");

        resetModelPathComboBox();

        modelPathComboBox.onChange = [this]
        {
            if (modelPathComboBox.getSelectedItemIndex() == -1)
            {
                DBG_AND_LOG("ModelSelectionWidget::modelPathComboBox::onChange: Combo box reset.");
            }
            else
            {
                if (modelPathComboBox.getSelectedItemIndex() == 0)
                {
                    DBG_AND_LOG(
                        "ModelSelectionWidget::modelPathComboBox::onChange: Custom path selected.");

                    openCustomPathPopup();
                }
                else
                {
                    lastSelectedPathIndex = modelPathComboBox.getSelectedItemIndex();

                    DBG_AND_LOG("ModelSelectionWidget::modelPathComboBox::onChange: Entry "
                                << lastSelectedPathIndex << " selected.");
                }

                loadModelButton.setEnabled(true);
            }
        };

        addAndMakeVisible(modelPathComboBox);

        modelPathComboBoxHandler.onMouseEnter = [this]()
        {
            if (instructionsMessage != nullptr)
            {
                instructionsMessage->setMessage(
                    "A drop-down menu with featured available models. Any custom paths "
                    "successfully loaded will automatically be added to the list.");
            }
        };
        modelPathComboBoxHandler.onMouseExit = [this]()
        {
            if (instructionsMessage != nullptr)
            {
                instructionsMessage->clearMessage();
            }
        };
        modelPathComboBoxHandler.attach();
    }

    void initializeLoadModelButton()
    {
        std::function<void()> loadCallback = [this]()
        {
            if (modelPathComboBox.getSelectedItemIndex() != 0)
            {
                selectedPath = modelPathComboBox.getText();

                if (selectedPath.contains(validPathBrokenTag))
                {
                    selectedPath = selectedPath.replace(validPathBrokenTag, "");
                }

                if (selectedPath.contains(validPathTryAgainTag))
                {
                    selectedPath = selectedPath.replace(validPathTryAgainTag, "");
                }

                if (selectedPath.contains(validPathErrorTag))
                {
                    selectedPath = selectedPath.replace(validPathErrorTag, "");
                }

                sendChangeMessage();
            }
        };

        // Mode when a model is selected and not currently being loaded (load enabled)
        loadButtonActiveInfo = MultiButton::Mode { "Load",
                                                   "Click to load currently selected model path.",
                                                   loadCallback,
                                                   MultiButton::DrawingMode::TextOnly };
        loadModelButton.addMode(loadButtonActiveInfo);
        loadModelButton.setMode(loadButtonActiveInfo.displayLabel);
        addAndMakeVisible(loadModelButton);
    }

    /**
     * Create callbacks for and launch the custom path popup.
     */
    void openCustomPathPopup(const String& prefillText = "")
    {
        std::function<void(String)> loadCallback = [this](String path)
        {
            DBG_AND_LOG("ModelSelectionWidget::openCustomPathPopup::loadCallback: "
                        << "Custom path \"" << path << "\" entered.");

            loadModelBypass(path);
        };

        std::function<void()> cancelCallback = [this]()
        {
            DBG_AND_LOG("ModelSelectionWidget::openCustomPathPopup::cancelCallback: "
                        << "Custom path selection canceled.");

            if (lastLoadedPathIndex >= 0)
            {
                // Set combo box selection to last successfully loaded model
                modelPathComboBox.setSelectedId(lastLoadedPathIndex + 1);
                modelPathComboBox.setEnabled(true);
            }
            else
            {
                resetState();
            }
        };

        CustomPathComponent* content =
            new CustomPathComponent(std::move(loadCallback), std::move(cancelCallback));

        if (prefillText.isNotEmpty())
        {
            content->setTextFieldValue(prefillText);
        }

        DialogWindow::LaunchOptions options;
        options.dialogTitle = "Enter Custom Path";
        options.dialogBackgroundColour = Colours::darkgrey;
        options.content.setOwned(content);

        options.useNativeTitleBar = false;
        options.resizable = false;
        options.escapeKeyTriggersCloseButton = true;
        options.componentToCentreAround = getParentComponent();

        options.launchAsync();
    }

    const float marginSize = 2;

    SharedResourcePointer<SharedChoices> sharedChoices;

    /* A ComboBox that always opens its popup from the top of the list, showing all
       items without scrolling to the currently-selected one first. Item 0 is the
       custom path entry, which must stay reachable however far down the list the
       loaded model sits. */
    struct FullListComboBox : public ComboBox
    {
        void showPopup() override
        {
            auto& lf = getLookAndFeel();
            auto label = std::unique_ptr<Label>(lf.createComboBoxTextBox(*this));

            auto menu = getRootMenu() ? *getRootMenu() : PopupMenu();

            /* The LookAndFeel asks for the selected item to be visible, which scrolls
               the popup to it. Overriding that with an id no item can have leaves
               nothing to scroll to, so the popup opens at the top showing every item. */
            PopupMenu::Options opts =
                lf.getOptionsForComboBoxPopupMenu(*this, *label).withItemThatMustBeVisible(0);

            // Guard against the ComboBox being destroyed while the menu is open,
            // as JUCE's own ComboBox::showPopup does via ModalCallbackFunction
            Component::SafePointer<FullListComboBox> safeThis(this);

            menu.showMenuAsync(opts,
                               [safeThis](int result)
                               {
                                   if (safeThis == nullptr)
                                       return;

                                   // Match JUCE's standard comboBoxPopupMenuFinishedCallback:
                                   // clear the popup-active flag before updating selection.
                                   safeThis->hidePopup();

                                   if (result != 0)
                                       safeThis->setSelectedId(result, sendNotification);
                               });
        }
    };

    FullListComboBox modelPathComboBox;
    HoverHandler modelPathComboBoxHandler { modelPathComboBox };

    int lastLoadedPathIndex; // Keep track of last loaded index for load failure cases
    int lastSelectedPathIndex;

    const std::string validPathErrorTag = " [ERROR]";
    const std::string validPathBrokenTag = " [DOWN]";
    const std::string validPathTryAgainTag = " [TRY AGAIN]";

    String selectedPath;

    MultiButton loadModelButton;
    MultiButton::Mode loadButtonActiveInfo;

    SharedResourcePointer<InstructionsMessage> instructionsMessage;
};
