/**
 * @file ModelTab.h
 * @brief Reusable component containing HARP GUI elements and state for a single model.
 * @author hugofloresgarcia, xribene, cwitkowitz
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "Model.h"

#include "widgets/ControlAreaWidget.h"
#include "widgets/ModelInfoWidget.h"
#include "widgets/ModelSelectionWidget.h"
#include "widgets/TrackAreaWidget.h"

#include "utils/Errors.h"
#include "utils/Logging.h"
#include "utils/Settings.h"
#include "utils/Tutorial.h"

using namespace juce;

class ModelTab : public Component, private ChangeListener, public ChangeBroadcaster
{
public:
    ModelTab()
    {
        modelSelectionWidget.addChangeListener(this);

        addAndMakeVisible(modelSelectionWidget);
        addAndMakeVisible(modelInfoWidget);
        addAndMakeVisible(controlAreaWidget);

        inputTracksLabel.setJustificationType(Justification::centred);
        inputTracksLabel.setFont(Font(20.0f, Font::bold));

        addAndMakeVisible(inputTracksLabel);
        addAndMakeVisible(inputTrackAreaWidget);

        initializeProcessCancelButton();

        outputTracksLabel.setJustificationType(Justification::centred);
        outputTracksLabel.setFont(Font(20.0f, Font::bold));

        addAndMakeVisible(outputTracksLabel);
        addAndMakeVisible(outputTrackAreaWidget);
    }

    ~ModelTab() { modelSelectionWidget.removeChangeListener(this); }

    // Accessor methods for WelcomeWindow tutorial
    std::shared_ptr<Model> getModel() const { return model; }
    String getLoadedPath() const { return model->getLoadedPath(); }

    void loadDefaultModel()
    {
        modelSelectionWidget.loadModelBypass(TutorialConstants::fallbackModelPath);
    }

    // Bounds accessors for tutorial steps
    Rectangle<int> getModelSelectBounds() const
    {
        return modelSelectionWidget.getBounds().expanded(2, 2);
    }

    Rectangle<int> getControlsBounds() const
    {
        auto bounds = controlAreaWidget.getBounds();

        if (bounds.getWidth() > 0 && bounds.getHeight() > 0)
            return bounds.expanded(2, 2);

        return {};
    }

    Rectangle<int> getInputFolderBounds()
    {
        auto bounds = inputTrackAreaWidget.getFirstTrackFolderButtonBounds();
        return getLocalArea(&inputTrackAreaWidget, bounds);
    }

    Rectangle<int> getInputPlayBounds()
    {
        auto bounds = inputTrackAreaWidget.getFirstTrackPlayButtonBounds();
        return getLocalArea(&inputTrackAreaWidget, bounds);
    }

    Rectangle<int> getInputTrackBounds() const { return inputTrackAreaWidget.getBounds(); }

    Rectangle<int> getProcessButtonBounds() const { return processCancelButton.getBounds(); }

    Rectangle<int> getTracksBounds() const
    {
        auto bounds = inputTrackAreaWidget.getBounds();
        if (outputTrackAreaWidget.isVisible())
            bounds = bounds.getUnion(outputTrackAreaWidget.getBounds());

        if (inputTracksLabel.isVisible())
            bounds = bounds.getUnion(inputTracksLabel.getBounds());
        if (outputTracksLabel.isVisible())
            bounds = bounds.getUnion(outputTracksLabel.getBounds());

        return bounds.expanded(2, 2);
    }

    bool isModelLoaded() { return model->isLoaded(); }

    void resized() override
    {
        FlexBox tabArea;
        tabArea.flexDirection = FlexBox::Direction::column;

        const int width = getWidth();

        /* Model Selection */

        tabArea.items.add(FlexItem(modelSelectionWidget)
                              .withHeight(modelSelectionRowHeight)
                              .withMinHeight(modelSelectionRowHeight)
                              .withMaxHeight(modelSelectionRowHeight)
                              .withFlex(0)
                              .withMargin(marginSize));

        /* Model Info */

        const int modelInfoHeight = modelInfoWidget.getPreferredHeightForWidth(width);

        tabArea.items.add(FlexItem(modelInfoWidget)
                              .withHeight((float) modelInfoHeight)
                              .withMinHeight((float) modelInfoHeight)
                              .withMargin(marginSize));

        /* Model Controls */

        if (controlAreaWidget.getNumControls() > 0)
        {
            int controlsHeight = getControlAreaRequiredHeightForTabWidth(width);

            tabArea.items.add(FlexItem(controlAreaWidget)
                                  .withHeight((float) controlsHeight)
                                  .withMinHeight((float) controlsHeight)
                                  .withMargin(marginSize));
        }
        else
        {
            controlAreaWidget.setBounds(0, 0, 0, 0);
        }

        const float totalTracks =
            inputTrackAreaWidget.getNumTracks() + outputTrackAreaWidget.getNumTracks();

        /* Input Tracks Area Widget */

        addTrackSection(tabArea,
                        inputTracksLabel,
                        inputTrackAreaWidget,
                        inputTrackAreaWidget.getNumTracks(),
                        totalTracks);

        /* Process / Cancel Button */

        FlexBox processCancelButtonRow;
        processCancelButtonRow.flexDirection = FlexBox::Direction::row;

        if (model->isLoaded())
        {
            processCancelButtonRow.items.add(FlexItem().withFlex(1));
            processCancelButtonRow.items.add(
                FlexItem(processCancelButton).withWidth(processButtonWidth).withMargin(marginSize));
            processCancelButtonRow.items.add(FlexItem().withFlex(1));

            tabArea.items.add(FlexItem(processCancelButtonRow)
                                  .withHeight(processButtonRowHeight)
                                  .withMinHeight(processButtonRowHeight)
                                  .withMaxHeight(processButtonRowHeight)
                                  .withFlex(0));
        }
        else
        {
            processCancelButton.setBounds(0, 0, 0, 0);
        }

        /* Output Tracks Area Widget */

        addTrackSection(tabArea,
                        outputTracksLabel,
                        outputTrackAreaWidget,
                        outputTrackAreaWidget.getNumTracks(),
                        totalTracks);

        tabArea.performLayout(getLocalBounds());
    }

    int getMinimumRequiredControlWidth() { return controlAreaWidget.getMinimumRequiredWidth(); }

    int getMinimumRequiredHeightForWidth(int width)
    {
        int height = 0;

        height += modelSelectionRowHeight + 2 * marginSize;
        height += modelInfoWidget.getPreferredHeightForWidth(width) + 2 * marginSize;

        if (controlAreaWidget.getNumControls() > 0)
        {
            height += getControlAreaRequiredHeightForTabWidth(width) + 2 * marginSize;
        }

        if (inputTrackAreaWidget.getNumTracks() > 0)
        {
            height += trackSectionLabelHeight + 4 * marginSize
                      + getTrackAreaMinimumHeight(inputTrackAreaWidget.getNumTracks());
        }

        if (model->isLoaded())
        {
            height += processButtonRowHeight;
        }

        if (outputTrackAreaWidget.getNumTracks() > 0)
        {
            height += trackSectionLabelHeight + 4 * marginSize
                      + getTrackAreaMinimumHeight(outputTrackAreaWidget.getNumTracks());
        }

        return height;
    }

    void resetState()
    {
        model = std::make_shared<Model>();

        modelSelectionWidget.resetState();
        modelInfoWidget.resetState();
        controlAreaWidget.resetState();
        inputTrackAreaWidget.resetState();
        outputTrackAreaWidget.resetState();

        processCancelButton.setMode(processButtonInfo.displayLabel);
        processCancelButton.setEnabled(false);

        currentProcessID = 0;

        resized();
    }

private:
    void initializeProcessCancelButton()
    {
        // Mode when a model is loaded and not currently processing (process enabled)
        processButtonInfo =
            MultiButton::Mode { "Process",
                                "Click to execute model with selected parameters and inputs.",
                                [this] { processCallback(); },
                                MultiButton::DrawingMode::TextOnly };
        // Mode when a model is loaded and currently processing (cancel enabled)
        cancelButtonInfo = MultiButton::Mode { "Cancel",
                                               "Click to cancel processing.",
                                               [this] { cancelCallback(); },
                                               MultiButton::DrawingMode::TextOnly };

        processCancelButton.addMode(processButtonInfo);
        processCancelButton.addMode(cancelButtonInfo);
        processCancelButton.setMode(processButtonInfo.displayLabel);

        addAndMakeVisible(processCancelButton);
    }

    void changeListenerCallback(ChangeBroadcaster* source)
    {
        if (source == &modelSelectionWidget)
        {
            loadModelCallback();
        }
    }

    int getControlAreaRequiredHeightForTabWidth(int tabWidth) const
    {
        return jmax(minControlAreaHeight, controlAreaWidget.getRequiredHeightForWidth(tabWidth));
    }

    int getTrackAreaMinimumHeight(int numTracks) const
    {
        if (numTracks <= 0)
        {
            return 0;
        }

        const int perTrackWithMargin = minVisibleTrackHeight + 2 * marginSize;

        return numTracks * perTrackWithMargin;
    }

    void addTrackSection(FlexBox& box,
                         Label& label,
                         Component& trackArea,
                         int numTracks,
                         float totalTracks) const
    {
        if (numTracks > 0)
        {
            box.items.add(FlexItem(label)
                              .withHeight(trackSectionLabelHeight)
                              .withMinHeight(trackSectionLabelHeight)
                              .withMaxHeight(trackSectionLabelHeight)
                              .withFlex(0)
                              .withMargin(marginSize));

            float flex = 4.0f * (numTracks / totalTracks);

            int minHeight = getTrackAreaMinimumHeight(numTracks);

            box.items.add(FlexItem(trackArea)
                              .withFlex(flex)
                              .withMinHeight((float) minHeight)
                              .withMargin(marginSize));
        }
        else
        {
            label.setBounds(0, 0, 0, 0);
            trackArea.setBounds(0, 0, 0, 0);
        }
    }

    void openErrorPopup(const Error error, std::function<void()> onExit = {})
    {
        std::optional<String> openablePath = getOpenablePath(error);
        String errorMessage = toUserMessage(error);

        constexpr int buttonOpenURL = 1;
        constexpr int buttonOpenLogs = 2;
        constexpr int buttonSendReport = 3;
        constexpr int buttonOk = 0;

        String popupMessage = errorMessage
                              + "\n\nOptional: add context below before opening the issue.";

        if (errorPopupWindow != nullptr)
        {
            removeChildComponent(errorPopupWindow.get());
            errorPopupWindow.reset();
        }

        errorPopupWindow = std::make_unique<AlertWindow>("Error", popupMessage, AlertWindow::WarningIcon);
        AlertWindow* alertWindow = errorPopupWindow.get();

        auto bindButtonCallback = [alertWindow](const String& buttonText, std::function<void()> callback)
        {
            for (int i = 0; i < alertWindow->getNumChildComponents(); ++i)
            {
                if (auto* button = dynamic_cast<TextButton*>(alertWindow->getChildComponent(i)))
                {
                    if (button->getButtonText() == buttonText)
                    {
                        button->onClick = std::move(callback);
                        return;
                    }
                }
            }
        };

        if (openablePath.has_value())
        {
            alertWindow->addButton("Open URL", buttonOpenURL);
            bindButtonCallback("Open URL", [this, openablePath, onExit]
            {
                URL(*openablePath).launchInDefaultBrowser();

                if (onExit)
                {
                    onExit();
                }

                if (errorPopupWindow != nullptr)
                {
                    removeChildComponent(errorPopupWindow.get());
                    errorPopupWindow.reset();
                }
            });
        }

        alertWindow->addButton("Open Logs", buttonOpenLogs);
        bindButtonCallback("Open Logs", [this, onExit]
        {
            HARPLogger::getInstance()->getLogFile().revealToUser();

            if (onExit)
            {
                onExit();
            }

            if (errorPopupWindow != nullptr)
            {
                removeChildComponent(errorPopupWindow.get());
                errorPopupWindow.reset();
            }
        });

        alertWindow->addButton("Open GitHub Issue", buttonSendReport);
        bindButtonCallback("Open GitHub Issue", [this, error, errorMessage, onExit]
        {
            if (errorPopupWindow != nullptr)
            {
                openGitHubIssue(
                    error, errorMessage, errorPopupWindow->getTextEditorContents("errorReportNotes"));
            }

            if (onExit)
            {
                onExit();
            }

            if (errorPopupWindow != nullptr)
            {
                removeChildComponent(errorPopupWindow.get());
                errorPopupWindow.reset();
            }
        });

        alertWindow->addButton("Ok", buttonOk);
        bindButtonCallback("Ok", [this, onExit]
        {
            if (onExit)
            {
                onExit();
            }

            if (errorPopupWindow != nullptr)
            {
                removeChildComponent(errorPopupWindow.get());
                errorPopupWindow.reset();
            }
        });

        alertWindow->addTextEditor(
            "errorReportNotes",
            "",
            "What were you doing when this happened? (optional)");

        addAndMakeVisible(*errorPopupWindow);
        errorPopupWindow->setAlwaysOnTop(true);

        int popupWidth = jmax(560, jmin(getWidth() - 24, 1100));
        int popupHeight = 280;
        errorPopupWindow->setBounds(getLocalBounds().withSizeKeepingCentre(popupWidth, popupHeight));
        errorPopupWindow->toFront(true);
    }

    void openGitHubIssue(const Error& error, const String& errorMessage, const String& notes)
    {
        static const String issueBaseUrl = "https://github.com/TEAMuP-dev/HARP/issues/new";
        static const String issueTemplate = "runtime_error_report.md";

        File logFile = HARPLogger::getInstance()->getLogFile();

        String issueTitle = "HARP runtime error report";
        if (const auto* gradioError = std::get_if<GradioError>(&error))
        {
            if (gradioError->reason.isNotEmpty())
            {
                issueTitle = "HARP: " + gradioError->reason;
            }
            else if (gradioError->type == GradioError::Type::QuotaExceeded)
            {
                issueTitle = "HARP: Hugging Face quota exceeded";
            }
        }

        String body;
        body << "## Summary\n";
        body << errorMessage << "\n\n";

        if (const auto* gradioError = std::get_if<GradioError>(&error))
        {
            if (gradioError->endpointPath.isNotEmpty())
            {
                body << "## Endpoint\n";
                body << gradioError->endpointPath << "\n\n";
            }
        }

        body << "## User Notes\n";
        body << (notes.isNotEmpty() ? notes : "(none)") << "\n\n";
        body << "## Environment\n";
        body << "- HARP version: " << JUCE_APPLICATION_VERSION_STRING << "\n";
        body << "- Time (local): " << Time::getCurrentTime().toString(true, true) << "\n";
        body << "- Log file: " << logFile.getFullPathName() << "\n\n";
        body << "## Reproduction\n";
        body << "1. \n";
        body << "2. \n";
        body << "3. \n";

        String escapedTitle = URL::addEscapeChars(issueTitle, true);
        String escapedBody = URL::addEscapeChars(body, true);
        String escapedTemplate = URL::addEscapeChars(issueTemplate, true);
        URL(issueBaseUrl + "?template=" + escapedTemplate + "&title=" + escapedTitle + "&body="
            + escapedBody)
            .launchInDefaultBrowser();
    }

    void loadModelCallback()
    {
        modelSelectionWidget.setDisabled();

        // Disable processing until model is loaded
        processCancelButton.setEnabled(false);

        // Obtain currently selected path
        String selectedPath = modelSelectionWidget.getCurrentlySelectedPath();

        DBG_AND_LOG("ModelTab::loadModelCallback: Attempting to load path \"" << selectedPath
                                                                              << "\".");

        loadingThreadPool.addJob(
            [this, selectedPath]
            {
                OpResult result = model->load(selectedPath);

                // Perform updates on message (GUI) thread
                MessageManager::callAsync(
                    [this, result]
                    {
                        if (result.wasOk())
                        {
                            modelSelectionWidget.setSuccessfulState();

                            modelInfoWidget.updateLabels(model->getMetadata());
                            modelInfoWidget.addOpenablePath(model->getOpenablePath());

                            controlAreaWidget.updateControls(model->getControls());

                            inputTrackAreaWidget.updateTracks(model->getInputTracks());
                            outputTrackAreaWidget.updateTracks(model->getOutputTracks());

                            resized();

                            sendSynchronousChangeMessage();

                            // Re-enable processing immediately
                            processCancelButton.setEnabled(true);
                        }
                        else
                        {
                            const Error error = result.getError();

                            std::function<void()> onExit = [this, error]
                            {
                                modelSelectionWidget.setUnsuccessfulState(error);

                                // Re-enable processing after closing error window
                                processCancelButton.setEnabled(true);
                            };

                            openErrorPopup(error, onExit);
                        }
                    });
            });
    }

    void processCallback()
    {
        std::map<Uuid, File> loadedInputFiles;

        for (std::unique_ptr<MediaDisplayComponent>& inputTrack :
             inputTrackAreaWidget.getMediaDisplays())
        {
            if (inputTrack->isRequired() && ! inputTrack->isFileLoaded())
            {
                // Make sure all required inputs have been set
                AlertWindow::showMessageBoxAsync(
                    AlertWindow::WarningIcon,
                    "Error",
                    "Required input track \"" + inputTrack->getTrackName()
                        + "\" is empty. Please load a file before processing.");

                return;
            }
            else if (inputTrack->isFileLoaded())
            {
                loadedInputFiles[inputTrack->getTrackID()] =
                    inputTrack->getOriginalFilePath().getLocalFile();
            }
            else
            {
                // Optional track skipped
            }
        }

        modelSelectionWidget.setDisabled();
        processCancelButton.setMode(cancelButtonInfo.displayLabel);

        // Switch choose-file button to inactive mode on all tracks during processing
        inputTrackAreaWidget.setLoadTrackEnabled(false);

        uint64_t processID = currentProcessID;

        processingThreadPool.addJob(
            [this, loadedInputFiles, processID]
            {
                std::vector<File> outputFiles;
                LabelList labels;

                DBG_AND_LOG("ModelTab::processCallback: Starting process \"" + String(processID)
                            + "\".");

                OpResult result = model->process(loadedInputFiles, outputFiles, labels);

                if (processID != currentProcessID.load())
                {
                    DBG_AND_LOG("ModelTab::processCallback: Ignoring result of stale process \""
                                + String(processID) + "\".");

                    return;
                }

                auto outputFilesPtr = std::make_shared<std::vector<File>>(std::move(outputFiles));
                auto labelsPtr = std::make_shared<LabelList>(std::move(labels));

                // Perform updates on message (GUI) thread
                MessageManager::callAsync(
                    [this, result, outputFilesPtr, labelsPtr]
                    {
                        std::function<void()> onExit = [this]
                        {
                            // Re-enable processing immediately
                            modelSelectionWidget
                                .setFinishedState(); // TODO - should this be last selected?
                            processCancelButton.setMode(processButtonInfo.displayLabel);

                            // Switch choose-file button back to active on all tracks
                            inputTrackAreaWidget.setLoadTrackEnabled(true);
                        };

                        if (result.wasOk())
                        {
                            auto& outputMediaDisplays = outputTrackAreaWidget.getMediaDisplays();

                            for (size_t i = 0; i < outputMediaDisplays.size(); ++i)
                            {
                                outputMediaDisplays[i]->initializeDisplay(
                                    URL((*outputFilesPtr)[i]));
                                outputMediaDisplays[i]->addLabels(*labelsPtr);
                            }

                            onExit();
                        }
                        else
                        {
                            openErrorPopup(result.getError(), onExit);
                        }
                    });
            });
    }

    void cancelCallback()
    {
        processCancelButton.setEnabled(false);

        DBG_AND_LOG("ModelTab::cancelCallback: Canceling process \"" + String(currentProcessID)
                    + "\".");

        // Invalidate any in-flight jobs
        ++currentProcessID;

        OpResult result = model->cancel();

        if (result.failed())
        {
            openErrorPopup(result.getError());
        }

        // Re-enable processing immediately
        modelSelectionWidget.setFinishedState(); // TODO - should this be last selected?

        processCancelButton.setMode(processButtonInfo.displayLabel);
        processCancelButton.setEnabled(true);

        // Switch choose-file button back to active on all tracks
        inputTrackAreaWidget.setLoadTrackEnabled(true);
    }

    static constexpr float marginSize = 2;

    static constexpr int modelSelectionRowHeight = 30;
    static constexpr int minControlAreaHeight = 96;
    static constexpr int processButtonWidth = 150;
    static constexpr int processButtonRowHeight = 30;
    static constexpr int trackSectionLabelHeight = 20;
    static constexpr int minVisibleTrackHeight = 50;

    std::shared_ptr<Model> model { new Model() };

    ModelSelectionWidget modelSelectionWidget;
    ModelInfoWidget modelInfoWidget;
    ControlAreaWidget controlAreaWidget;

    Label inputTracksLabel { "Input Tracks", "Input Tracks" };
    TrackAreaWidget inputTrackAreaWidget { DisplayMode::Input };

    MultiButton processCancelButton;
    MultiButton::Mode processButtonInfo;
    MultiButton::Mode cancelButtonInfo;

    Label outputTracksLabel { "Output Tracks", "Output Tracks" };
    TrackAreaWidget outputTrackAreaWidget { DisplayMode::Output };

    ThreadPool loadingThreadPool { 1 };
    ThreadPool processingThreadPool { 10 };

    std::atomic<uint64_t> currentProcessID { 0 };
    std::unique_ptr<AlertWindow> errorPopupWindow;
};
