/**
 * @file ModelTab.h
 * @brief Reusable component containing HARP GUI elements and state for a single model.
 * @author hugofloresgarcia, xribene, cwitkowitz, saumya-pailwan
 */

#pragma once

#include <cmath>

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

        // Weighting is by flexible tracks, so the total has to be of those too
        const float totalTracks = (float) (inputTrackAreaWidget.getNumFlexibleTracks()
                                           + outputTrackAreaWidget.getNumFlexibleTracks());

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

        positionErrorPopup();
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

        // Publish the empty state so the status area does not keep
        // showing the previous model's last status
        model->setStatus(ModelStatus::EMPTY);

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

        // The track area applies its own margins, so it owns this calculation
        return TrackAreaWidget::getRequiredHeightForTracks(numTracks);
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

            /* Weight by the tracks that actually stretch. Counting a fixed-height
               track (a generic file picker) as a full share of flexible space gives
               its section too much, so the flexible tracks beside it end up taller
               than those in the other section. */
            auto* area = dynamic_cast<const TrackAreaWidget*>(&trackArea);

            const int flexibleTracks = area != nullptr ? area->getNumFlexibleTracks() : numTracks;
            const int fixedHeight = area != nullptr ? area->getFixedTracksHeight() : 0;

            float flex = totalTracks > 0.0f ? 4.0f * ((float) flexibleTracks / totalTracks) : 0.0f;

            int minHeight = getTrackAreaMinimumHeight(flexibleTracks) + fixedHeight;

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

        DBG_AND_LOG("ModelTab::openErrorPopup: " + toLogString(error));

        // Determine whether this error warrants a GitHub bug report.
        // Quota errors, invalid paths, and expected HTTP failures are user-actionable
        // and don't need a report; only unexpected runtime/parse errors do.
        bool isReportableError = false;
        if (const auto* gradioErr = std::get_if<GradioError>(&error))
            isReportableError = (gradioErr->type == GradioError::Type::RuntimeError);
        else if (std::get_if<JsonError>(&error) || std::get_if<ControlError>(&error))
            isReportableError = true;

        // If a popup is being replaced, its pending cleanup must still run so the
        // widgets it was responsible for re-enabling do not stay disabled
        dismissErrorPopup();

        errorPopupWindow = std::make_unique<BottomButtonAlertWindow>(
            "Error", errorMessage, AlertWindow::WarningIcon);
        centredAlertLF.messageText = errorMessage;
        errorPopupWindow->setLookAndFeel(&centredAlertLF);
        errorPopupOnExit = std::move(onExit);

        auto addPopupButton = [this](const String& buttonText, std::function<void()> callback)
        {
            errorPopupWindow->addButton(buttonText, 0);

            if (auto* button = errorPopupWindow->getButton(buttonText))
                button->onClick = std::move(callback);
        };

        if (openablePath.has_value())
        {
            addPopupButton("Open URL",
                           [openablePath] { URL(*openablePath).launchInDefaultBrowser(); });
        }

        addPopupButton("Open Logs",
                       [] { HARPLogger::getInstance()->getLogFile().revealToUser(); });

        if (isReportableError)
        {
            addPopupButton("Report",
                           [this, error, errorMessage]
                           {
                               // Open GitHub issue but keep the popup open
                               openGitHubIssue(error, errorMessage, "");
                           });
        }

        addPopupButton("Ok", [this] { dismissErrorPopup(); });

        addAndMakeVisible(*errorPopupWindow);
        errorPopupWindow->setAlwaysOnTop(true);

        positionErrorPopup();
        errorPopupWindow->toFront(true);
    }

    void dismissErrorPopup()
    {
        if (errorPopupOnExit)
        {
            // Clear before invoking in case the callback opens another popup
            auto pendingOnExit = std::move(errorPopupOnExit);
            errorPopupOnExit = {};
            pendingOnExit();
        }

        if (errorPopupWindow != nullptr)
        {
            removeChildComponent(errorPopupWindow.get());
            errorPopupWindow->setVisible(false);
            errorPopupWindow->setLookAndFeel(nullptr);

            /* Defer destruction: this can be reached from one of the popup's own
               button callbacks, and a Button must not be destroyed from inside
               its own onClick. The lambda holds the last reference and releases
               it on the message thread. */
            std::shared_ptr<BottomButtonAlertWindow> oldPopup = std::move(errorPopupWindow);
            MessageManager::callAsync([oldPopup] {});
        }
    }

    void positionErrorPopup()
    {
        if (errorPopupWindow == nullptr)
        {
            return;
        }

        Component* topLevel = getTopLevelComponent();

        // Size based on full window width so the popup is never squashed when
        // the media clipboard panel is open and ModelTab is narrow.
        int windowWidth = (topLevel != nullptr) ? topLevel->getWidth() : getWidth();
        int windowHeight = (topLevel != nullptr) ? topLevel->getHeight() : getHeight();
        int popupWidth = jmin(520, windowWidth - 24);

        /* Measure the wrapped message so the popup is tall enough to show all
           of it, using the same font and insets as CentredAlertLookAndFeel */
        AttributedString attrStr;
        attrStr.append(centredAlertLF.messageText, Font(popupMessageFontHeight));

        TextLayout layout;
        layout.createLayout(attrStr,
                            (float) (popupWidth - 2 * (popupEdgeGap + popupIconWidth)));

        const int buttonH = centredAlertLF.getAlertWindowButtonHeight();
        int popupHeight = popupEdgeGap + popupTitleHeight + (int) std::ceil(layout.getHeight())
                          + popupEdgeGap + buttonH + popupButtonBottomPadding;
        popupHeight = jlimit(180, jmax(180, windowHeight - 24), popupHeight);

        // Find the window's center in screen space, then convert to ModelTab's
        // local coordinate space so the popup is centered in the full window
        // regardless of where ModelTab sits within it.
        Point<int> windowCentreScreen =
            (topLevel != nullptr)
                ? topLevel->localPointToGlobal(topLevel->getLocalBounds().getCentre())
                : localPointToGlobal(getLocalBounds().getCentre());
        Point<int> centreInLocal = getLocalPoint(nullptr, windowCentreScreen);

        errorPopupWindow->setBounds(
            Rectangle<int>(popupWidth, popupHeight).withCentre(centreInLocal));
    }

    void openGitHubIssue(const Error& error, const String& errorMessage, const String& notes)
    {
        static const String issueBaseUrl = "https://github.com/TEAMuP-dev/HARP/issues/new";
        static const String issueTemplate = "runtime_error_report.yml";

        String issueTitle = "HARP runtime error report";
        String endpointPath;

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

            endpointPath = gradioError->endpointPath;
        }

        String environment;
        environment << "- HARP version: " << JUCE_APPLICATION_VERSION_STRING << "\n";
        environment << "- Time (local): " << Time::getCurrentTime().toString(true, true) << "\n";
        environment << "- Log file: " << HARPLogger::getInstance()->getLogFile().getFullPathName();

        /* Only values are supplied here; the report's structure lives solely in
           the issue form, whose field ids these query parameters correspond to.
           See .github/ISSUE_TEMPLATE/runtime_error_report.yml */
        StringPairArray fields;
        fields.set("title", issueTitle);
        fields.set("summary", errorMessage);
        fields.set("environment", environment);

        if (endpointPath.isNotEmpty())
        {
            fields.set("endpoint", endpointPath);
        }

        if (notes.isNotEmpty())
        {
            fields.set("notes", notes);
        }

        String query = "?template=" + URL::addEscapeChars(issueTemplate, true);

        for (const auto& key : fields.getAllKeys())
        {
            query += "&" + key + "=" + URL::addEscapeChars(fields[key], true);
        }

        URL(issueBaseUrl + query).launchInDefaultBrowser();
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
                            modelSelectionWidget.setSuccessfulState(model->getLoadedPath());

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

        for (const auto& controlInfo : model->getControls())
        {
            if (auto* fileInfo = dynamic_cast<FileComponentInfo*>(controlInfo.get()))
            {
                if (fileInfo->required && fileInfo->path.empty())
                {
                    AlertWindow::showMessageBoxAsync(
                        AlertWindow::WarningIcon,
                        "Error",
                        "Required file input \"" + String(fileInfo->label)
                            + "\" is empty. Please select a file before processing.");

                    return;
                }
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

                            for (size_t i = 0;
                                 i < outputMediaDisplays.size() && i < outputFilesPtr->size();
                                 ++i)
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

    // Shared popup layout metrics, used by CentredAlertLookAndFeel when drawing
    // and by positionErrorPopup when measuring the required popup height
    static constexpr int popupButtonBottomPadding = 24;
    static constexpr int popupEdgeGap = 10;
    static constexpr int popupIconWidth = 80;
    static constexpr int popupTitleHeight = 24;
    static constexpr float popupMessageFontHeight = 16.0f;

    /* LookAndFeel override that draws the AlertWindow message text centered.
    We re-derive both from the actual window height here so that
    text fills the real available space and buttons are accounted for at the
    bottom where BottomButtonAlertWindow will move them. */

    struct CentredAlertLookAndFeel : public LookAndFeel_V4
    {
        String messageText; // set before showing the popup

        void drawAlertBox(Graphics& g,
                          AlertWindow& alert,
                          const Rectangle<int>& /*textArea*/,
                          TextLayout& /*unused*/) override
        {
            // Background
            auto cornerSize = 4.0f;
            g.setColour(alert.findColour(AlertWindow::outlineColourId));
            g.drawRoundedRectangle(alert.getLocalBounds().toFloat(), cornerSize, 2.0f);

            auto bounds = alert.getLocalBounds().reduced(1);
            g.reduceClipRegion(bounds);

            g.setColour(alert.findColour(AlertWindow::backgroundColourId));
            g.fillRoundedRectangle(bounds.toFloat(), cornerSize);

            // Icon
            auto iconSpaceUsed = 0;
            const auto iconWidth = popupIconWidth;
            auto iconSize = jmin(iconWidth + 50, bounds.getHeight() + 20);

            if (alert.containsAnyExtraComponents() || alert.getNumButtons() > 2)
                iconSize = jmin(iconSize, 200);

            Rectangle<int> iconRect(iconSize / -10, iconSize / -10, iconSize, iconSize);

            if (alert.getAlertType() != MessageBoxIconType::NoIcon)
            {
                Path icon;
                char character;
                uint32 color;

                if (alert.getAlertType() == MessageBoxIconType::WarningIcon)
                {
                    character = '!';
                    icon.addTriangle((float) iconRect.getX() + (float) iconRect.getWidth() * 0.5f,
                                     (float) iconRect.getY(),
                                     (float) iconRect.getRight(),
                                     (float) iconRect.getBottom(),
                                     (float) iconRect.getX(),
                                     (float) iconRect.getBottom());
                    icon = icon.createPathWithRoundedCorners(5.0f);
                    color = 0x66ff2a00;
                }
                else
                {
                    color = Colour(0xff00b0b9).withAlpha(0.4f).getARGB();
                    character = alert.getAlertType() == MessageBoxIconType::InfoIcon ? 'i' : '?';
                    icon.addEllipse(iconRect.toFloat());
                }

                GlyphArrangement ga;
                ga.addFittedText({ (float) iconRect.getHeight() * 0.9f, Font::bold },
                                 String::charToString((juce_wchar) (uint8) character),
                                 (float) iconRect.getX(),
                                 (float) iconRect.getY(),
                                 (float) iconRect.getWidth(),
                                 (float) iconRect.getHeight(),
                                 Justification::centred,
                                 false);
                ga.createPath(icon);
                icon.setUsingNonZeroWinding(false);
                g.setColour(Colour(color));
                g.fillPath(icon);

                iconSpaceUsed = iconWidth;
            }

            if (messageText.isNotEmpty())
            {
                const int buttonH = getAlertWindowButtonHeight();
                const int titleH = popupTitleHeight;
                const int edgeGap = popupEdgeGap;
                const int bottomOfText =
                    alert.getHeight() - buttonH - popupButtonBottomPadding - edgeGap;

                const int rightPadding = edgeGap + iconSpaceUsed;
                Rectangle<float> fullTextArea(
                    (float) (edgeGap + iconSpaceUsed),
                    (float) (edgeGap + titleH),
                    (float) (alert.getWidth() - edgeGap - iconSpaceUsed - rightPadding),
                    (float) (bottomOfText - (edgeGap + titleH)));

                Font msgFont(popupMessageFontHeight);

                AttributedString attrStr;
                attrStr.setJustification(Justification::topLeft);
                attrStr.append(messageText, msgFont, alert.findColour(AlertWindow::textColourId));

                TextLayout layout;
                layout.createLayout(attrStr, fullTextArea.getWidth());
                layout.draw(g, fullTextArea);
            }
        }
    };

    class BottomButtonAlertWindow : public AlertWindow
    {
    public:
        BottomButtonAlertWindow(const String& title,
                                const String& message,
                                MessageBoxIconType iconType)
            : AlertWindow(title, message, iconType)
        {
        }

        void resized() override
        {
            const int buttonH = getLookAndFeel().getAlertWindowButtonHeight();
            const int targetY = getHeight() - popupButtonBottomPadding - buttonH;
            const int spacer = 16;

            Array<TextButton*> btns;
            for (int i = 0; i < getNumChildComponents(); ++i)
                if (auto* btn = dynamic_cast<TextButton*>(getChildComponent(i)))
                    btns.add(btn);

            int totalWidth = -spacer;
            for (auto* btn : btns)
                totalWidth += btn->getWidth() + spacer;

            int x = (getWidth() - totalWidth) / 2;
            for (auto* btn : btns)
            {
                btn->setTopLeftPosition(x, targetY);
                x += btn->getWidth() + spacer;
            }
        }
    };

    static constexpr float marginSize = 2;

    static constexpr int modelSelectionRowHeight = 30;
    static constexpr int minControlAreaHeight = 96;
    static constexpr int processButtonWidth = 150;
    static constexpr int processButtonRowHeight = 30;
    static constexpr int trackSectionLabelHeight = 20;

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
    CentredAlertLookAndFeel centredAlertLF;
    std::unique_ptr<BottomButtonAlertWindow> errorPopupWindow;
    std::function<void()> errorPopupOnExit;
};
