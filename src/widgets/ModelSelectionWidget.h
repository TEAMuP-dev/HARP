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

        setSize(400, 80);
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

        FlexBox fullPopup;
        fullPopup.flexDirection = FlexBox::Direction::column;

        fullPopup.items.add(FlexItem(pathEditor).withHeight(30).withMargin(2));

        FlexBox buttonsArea;
        buttonsArea.flexDirection = FlexBox::Direction::row;

        buttonsArea.items.add(FlexItem(loadButton).withFlex(1).withMargin(10));
        buttonsArea.items.add(FlexItem().withFlex(0.25));
        buttonsArea.items.add(FlexItem(cancelButton).withFlex(1).withMargin(10));

        fullPopup.items.add(FlexItem(buttonsArea).withFlex(1));

        fullPopup.performLayout(fullArea);
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

    void setSuccessfulState()
    {
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
            if (e->type == HttpError::Type::BadStatusCode && e->statusCode == 503)
            {
                updatedEntry += validPathBrokenTag;
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

            selectedPath = path;
            sendChangeMessage();
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

    ComboBox modelPathComboBox;
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
