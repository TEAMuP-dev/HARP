/**
 * @file ModelTab.h
 * @brief Reusable component containing HARP GUI elements and state for a single model.
 * @author cwitkowitz, saumya-pailwan, VedMistry42, xribene, hugofloresgarcia
 */

#pragma once

#include <cmath>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Model.h"

#include "widgets/ControlAreaWidget.h"
#include "widgets/ModelInfoWidget.h"
#include "widgets/TrackAreaWidget.h"

#include "utils/Errors.h"
#include "utils/Logging.h"
#include "utils/ModelCatalog.h"
#include "utils/Settings.h"

#include "windows/tutorial/TutorialTargets.h"

using namespace juce;

/**
 * Sends a change message once a model has loaded, and once the error from a failed load
 * has been dismissed. A tab whose first load failed is then left without a model.
 */
class ModelTab : public Component, public ChangeBroadcaster
{
public:
    ModelTab()
    {
        addAndMakeVisible(modelInfoWidget);
        addAndMakeVisible(controlAreaWidget);

        addAndMakeVisible(inputTracksLabel);
        addAndMakeVisible(inputTrackAreaWidget);

        initializeProcessCancelButton();

        addAndMakeVisible(outputTracksLabel);
        addAndMakeVisible(outputTrackAreaWidget);

        controlAreaWidget.setComponentID(TutorialTargets::controls);
        inputTrackAreaWidget.setComponentID(TutorialTargets::inputTracks);
        outputTrackAreaWidget.setComponentID(TutorialTargets::outputTracks);
        processCancelButton.setComponentID(TutorialTargets::processButton);
    }

    std::shared_ptr<Model> getModel() const { return model; }

    // Loads a model in the background, reporting the outcome with a change message
    void loadModel(const String& modelPath)
    {
        loading = true;

        // Disable processing until model is loaded
        processCancelButton.setEnabled(false);

        const String pathToLoad = canonicalizeModelPath(modelPath);

        DBG_AND_LOG("ModelTab::loadModel: Attempting to load path \"" << pathToLoad << "\".");

        SafePointer<ModelTab> safeThis(this);

        loadingThreadPool.addJob(
            [this, safeThis, pathToLoad]
            {
                OpResult result = model->load(pathToLoad);

                // Perform updates on message (GUI) thread
                MessageManager::callAsync(
                    [safeThis, result, pathToLoad]
                    {
                        if (safeThis != nullptr)
                            safeThis->finishLoading(result, pathToLoad);
                    });
            });
    }

    // True from the moment a load is requested until its outcome has been reported
    bool isLoading() const { return loading; }

    bool isModelLoaded() { return model->isLoaded(); }

    // True while a model load or process request is still queued or running on
    // one of this tab's thread pools. Used to defer destruction of the tab: if
    // its ThreadPool is destroyed while a worker is blocked in a network call,
    // JUCE force-kills the worker thread, which can corrupt the shared
    // networking subsystem and hang all future requests.
    bool hasPendingRequests() const
    {
        return loadingThreadPool.getNumJobs() > 0 || processingThreadPool.getNumJobs() > 0;
    }

    // Called once this tab has left the UI but cannot be destroyed yet because a
    // request is still in flight. Aborts the connection locally so that the
    // request settles within moments instead of whenever the server or the
    // request timeout gets around to it - which may be long after the app has
    // started shutting down, at which point delivering the result crashes. Any
    // result that does still arrive is dropped, since the user closed this tab.
    void abandon()
    {
        abandoned = true;

        // Invalidate any in-flight jobs
        ++currentProcessID;

        model->abortActiveRequests();
    }

    void resized() override
    {
        FlexBox tabArea;
        tabArea.flexDirection = FlexBox::Direction::column;

        const auto contentArea = getLocalBounds().reduced(pagePadding);
        const int width = contentArea.getWidth();

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

        tabArea.performLayout(contentArea);

        positionErrorPopup();
    }

    int getMinimumRequiredControlWidth() { return controlAreaWidget.getMinimumRequiredWidth(); }

    int getMinimumRequiredHeightForWidth(int width)
    {
        width -= 2 * pagePadding;

        int height = 2 * pagePadding;

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
                         Component& label,
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
        // and do not need a report. Only unexpected runtime and parse errors do.
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

        addPopupButton("Open Logs", [] { HARPLogger::getInstance()->getLogFile().revealToUser(); });

        if (isReportableError)
        {
            addPopupButton("Report",
                           [this, error, errorMessage]
                           {
                               // Open GitHub issue but keep the popup open
                               openGitHubIssue(error, errorMessage);
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
        layout.createLayout(attrStr, (float) (popupWidth - 2 * (popupEdgeGap + popupIconWidth)));

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

    void openGitHubIssue(const Error& error, const String& errorMessage)
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

        /* Only values are supplied here. The report's structure lives solely in
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

        String query = "?template=" + URL::addEscapeChars(issueTemplate, true);

        for (const auto& key : fields.getAllKeys())
        {
            query += "&" + key + "=" + URL::addEscapeChars(fields[key], true);
        }

        URL(issueBaseUrl + query).launchInDefaultBrowser();
    }

    void finishLoading(const OpResult& result, const String& requestedPath)
    {
        if (abandoned)
        {
            // Tab was closed while this load was in flight
            return;
        }

        if (result.wasOk())
        {
            loading = false;

            catalog->recordLoadSuccess(model->getLoadedPath(), model->getMetadata());

            modelInfoWidget.updateLabels(model->getMetadata());
            modelInfoWidget.addOpenablePath(model->getOpenablePath());

            // Once loaded, only how the model is deployed is worth noting
            if (const auto* entry = catalog->findEntry(model->getLoadedPath()))
                modelInfoWidget.setBadges(ModelStyle::getBadges(*entry, false));

            controlAreaWidget.updateControls(model->getControls());

            inputTrackAreaWidget.updateTracks(model->getInputTracks());
            outputTrackAreaWidget.updateTracks(model->getOutputTracks());

            resized();

            // Enable processing now that a model is loaded
            processCancelButton.setEnabled(true);

            sendSynchronousChangeMessage();
        }
        else
        {
            const Error error = result.getError();

            catalog->recordLoadFailure(requestedPath, error);

            // The outcome is reported once the error has been seen
            openErrorPopup(error,
                           [this]
                           {
                               loading = false;
                               sendChangeMessage();
                           });
        }
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

        processCancelButton.setMode(cancelButtonInfo.displayLabel);

        // Switch choose-file button to inactive mode on all tracks during processing
        inputTrackAreaWidget.setLoadTrackEnabled(false);

        uint64_t processID = currentProcessID;

        SafePointer<ModelTab> safeThis(this);

        processingThreadPool.addJob(
            [this, safeThis, loadedInputFiles, processID]
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
                    [safeThis, result, outputFilesPtr, labelsPtr]
                    {
                        if (safeThis != nullptr)
                            safeThis->finishProcessing(result, *outputFilesPtr, *labelsPtr);
                    });
            });
    }

    void finishProcessing(const OpResult& result,
                          const std::vector<File>& outputFiles,
                          const LabelList& labels)
    {
        if (abandoned)
        {
            // Tab was closed while this process was in flight
            return;
        }

        std::function<void()> onExit = [this]
        {
            // Re-enable processing immediately
            processCancelButton.setMode(processButtonInfo.displayLabel);

            // Switch choose-file button back to active on all tracks
            inputTrackAreaWidget.setLoadTrackEnabled(true);
        };

        if (result.wasOk())
        {
            auto& outputMediaDisplays = outputTrackAreaWidget.getMediaDisplays();

            for (size_t i = 0; i < outputMediaDisplays.size() && i < outputFiles.size(); ++i)
            {
                outputMediaDisplays[i]->initializeDisplay(URL(outputFiles[i]));
                outputMediaDisplays[i]->addLabels(labels);
            }

            onExit();
        }
        else
        {
            openErrorPopup(result.getError(), onExit);
        }
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

    /* LookAndFeel override that lays the AlertWindow message out over the whole
       window rather than JUCE's default text area. The text area is re-derived from
       the actual window height, reserving the strip at the bottom that
       BottomButtonAlertWindow moves the buttons into. */
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
    // Space around the whole tab, matching the Home tab's
    static constexpr int pagePadding = 8;

    static constexpr int minControlAreaHeight = 96;
    static constexpr int processButtonWidth = 150;
    static constexpr int processButtonRowHeight = 30;
    static constexpr int trackSectionLabelHeight = ModelStyle::sectionHeaderHeight - 2;

    std::shared_ptr<Model> model { new Model() };

    ModelInfoWidget modelInfoWidget;
    ControlAreaWidget controlAreaWidget;

    ModelStyle::SectionHeader inputTracksLabel { "Input Tracks" };
    TrackAreaWidget inputTrackAreaWidget { DisplayMode::Input };

    MultiButton processCancelButton;
    MultiButton::Mode processButtonInfo;
    MultiButton::Mode cancelButtonInfo;

    ModelStyle::SectionHeader outputTracksLabel { "Output Tracks" };
    TrackAreaWidget outputTrackAreaWidget { DisplayMode::Output };

    ThreadPool loadingThreadPool { 1 };
    ThreadPool processingThreadPool { 10 };

    std::atomic<uint64_t> currentProcessID { 0 };
    std::atomic<bool> abandoned { false };
    bool loading = false;

    SharedResourcePointer<ModelCatalog> catalog;

    CentredAlertLookAndFeel centredAlertLF;
    std::unique_ptr<BottomButtonAlertWindow> errorPopupWindow;
    std::function<void()> errorPopupOnExit;
};
