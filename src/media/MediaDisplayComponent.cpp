#include "MediaDisplayComponent.h"

MediaDisplayComponent::MediaDisplayComponent() : MediaDisplayComponent("Media Track") {}

MediaDisplayComponent::MediaDisplayComponent(String name, bool req, DisplayMode mode)
    : trackName(name), required(req), displayMode(mode)
{
    formatManager.registerBasicFormats();

    deviceManager.initialise(0, 2, nullptr, true, {}, nullptr);
    deviceManager.addAudioCallback(&sourcePlayer);

    sourcePlayer.setSource(&transportSource);

    trackNameLabel.setText(trackName, dontSendNotification);
    trackNameLabel.setJustificationType(Justification::centred);
    headerComponent.addAndMakeVisible(trackNameLabel);
    initializeButtons();
    addAndMakeVisible(headerComponent);

    headerFlexBox.flexDirection = FlexBox::Direction::row;
    headerFlexBox.alignItems = FlexBox::AlignItems::stretch;

    buttonsFlexBox.flexDirection = FlexBox::Direction::column;
    buttonsFlexBox.alignItems = FlexBox::AlignItems::center;
    buttonsFlexBox.justifyContent = FlexBox::JustifyContent::center;

    horizontalScrollBar.setAutoHide(false);
    horizontalScrollBar.addListener(this);

    mediaAreaContainer.addAndMakeVisible(overheadPanel);
    mediaAreaContainer.addAndMakeVisible(mediaComponent);
    mediaAreaContainer.addAndMakeVisible(horizontalScrollBar);
    addAndMakeVisible(mediaAreaContainer);

    mediaAreaFlexBox.flexDirection = FlexBox::Direction::column;

    currentPositionMarker.setFill(Colours::white.withAlpha(0.85f));
    addAndMakeVisible(currentPositionMarker);

    resetPaths();
    resetScrollBar();
}

void MediaDisplayComponent::initializeButtons()
{
    // Mode when a playable file is loaded
    playButtonActiveInfo = MultiButton::Mode {
        "Play-Active",
        [this] { start(); },
        Colours::limegreen,
        "Click to start playback",
        MultiButton::DrawingMode::IconOnly,
        fontaudio::Play,
    };
    // Mode when there is nothing to play
    playButtonInactiveInfo = MultiButton::Mode {
        "Play-Inactive",
        [this] {},
        Colours::lightgrey,
        "Nothing to play",
        MultiButton::DrawingMode::IconOnly,
        fontaudio::Play,
    };
    // Mode during playback
    stopButtonInfo = MultiButton::Mode {
        "Stop",
        [this] { stop(); },
        Colours::orangered,
        "Click to stop playback",
        MultiButton::DrawingMode::IconOnly,
        fontaudio::Stop,
    };
    playStopButton.addMode(playButtonActiveInfo);
    playStopButton.addMode(playButtonInactiveInfo);
    playStopButton.addMode(stopButtonInfo);
    headerComponent.addAndMakeVisible(playStopButton);

    chooseButtonInfo = MultiButton::Mode {
        "ChooseFile",
        [this] { chooseFileCallback(); },
        Colours::lightblue,
        "Click to choose a media file",
        MultiButton::DrawingMode::IconOnly,
        fontawesome::Folder,
    };
    chooseFileButton.addMode(chooseButtonInfo);
    headerComponent.addAndMakeVisible(chooseFileButton);

    // Mode when an unsaved file is loaded
    saveButtonActiveInfo = MultiButton::Mode {
        "Save-Active",
        [this] { saveFileCallback(); },
        Colours::lightblue,
        "Click to save the media file",
        MultiButton::DrawingMode::IconOnly,
        fontawesome::Save,
    };
    // Mode when there is nothing to save
    saveButtonInactiveInfo = MultiButton::Mode {
        "Save-Inactive",
        [this] {},
        Colours::lightgrey,
        "Nothing to save",
        MultiButton::DrawingMode::IconOnly,
        fontawesome::Save,
    };
    saveFileButton.addMode(saveButtonActiveInfo);
    saveFileButton.addMode(saveButtonInactiveInfo);
    headerComponent.addAndMakeVisible(saveFileButton);

    resetButtonState();
}

MediaDisplayComponent::~MediaDisplayComponent()
{
    deviceManager.removeAudioCallback(&sourcePlayer);

    sourcePlayer.setSource(nullptr);

    horizontalScrollBar.removeListener(this);

    clearLabels();
}

void MediaDisplayComponent::paint(Graphics& g)
{
    g.fillAll(Colours::darkgrey);
    g.setColour(Colours::lightblue);

    if (! isFileLoaded())
    {
        g.setFont(14.0f);
        g.drawFittedText("No media file selected...", getLocalBounds(), Justification::centred, 2);
    }
}

void MediaDisplayComponent::resized()
{
    Rectangle<int> totalBounds = getLocalBounds();

    // Remove existing items in main flex
    mainFlexBox.items.clear();

    if (isThumbnailTrack())
    {
        // Place header over media
        mainFlexBox.flexDirection = FlexBox::Direction::column;
        // Fixed area for track label and buttons
        mainFlexBox.items.add(FlexItem(headerComponent)
                                  .withHeight(trackNameLabel.getFont().getHeight())
                                  .withMargin(1));
    }
    else
    {
        // Place header beside media
        mainFlexBox.flexDirection = FlexBox::Direction::row;
        // Fixed area for track label and buttons
        mainFlexBox.items.add(FlexItem(headerComponent).withFlex(1).withMaxWidth(40).withMargin(4));
    }

    // Media area takes remaining space
    mainFlexBox.items.add(FlexItem(mediaAreaContainer).withFlex(8));

    mainFlexBox.performLayout(totalBounds);

    // Remove existing items in header flex
    headerFlexBox.items.clear();

    // Add track label to header flex
    headerFlexBox.items.add(FlexItem(trackNameLabel).withFlex(1).withMargin({ 0, 2, 0, 0 }));

    if (! isThumbnailTrack())
    {
        // Add buttons to header flex
        headerFlexBox.items.add(FlexItem(buttonsFlexBox).withFlex(2).withMargin({ 0, 0, 0, 1 }));
    }

    // Perform layout of controls inside header
    headerFlexBox.performLayout(headerComponent.getLocalBounds());

    Rectangle<float> labelBounds = trackNameLabel.getBounds().toFloat();
    float trackNameLabelWidth = labelBounds.getWidth();
    float trackNameLabelHeight = labelBounds.getHeight();

    if (! isThumbnailTrack())
    {
        Point<float> labelCenter = labelBounds.getCentre();
        // Rotate track name label 90 degrees
        trackNameLabel.setTransform(
            AffineTransform::rotation(-MathConstants<float>::halfPi, labelCenter.x, labelCenter.y));

        // Swap width and height
        trackNameLabelWidth = labelBounds.getHeight();
        trackNameLabelHeight = labelBounds.getWidth();

        // Update track name label bounds and position
        trackNameLabel.setBounds(
            labelBounds.withSize(trackNameLabelWidth, trackNameLabelHeight).toNearestInt());
    }

    // Center track name label within header
    float labelX = (labelBounds.getWidth() - trackNameLabelWidth) / 2.0f;
    float labelY = (labelBounds.getHeight() - trackNameLabelHeight) / 2.0f;
    trackNameLabel.setTopLeftPosition(static_cast<int>(labelX), static_cast<int>(labelY));

    // Remove existing items in button flex
    buttonsFlexBox.items.clear();

    // Add buttons to flex with equal height
    buttonsFlexBox.items.add(
        FlexItem(playStopButton).withHeight(22).withWidth(22).withMargin({ 2, 0, 2, 0 }));
    if (isInputTrack())
    {
        buttonsFlexBox.items.add(
            FlexItem(chooseFileButton).withHeight(22).withWidth(22).withMargin({ 2, 0, 2, 0 }));
    }
    if (isOutputTrack())
    {
        buttonsFlexBox.items.add(
            FlexItem(saveFileButton).withHeight(22).withWidth(22).withMargin({ 2, 0, 2, 0 }));
    }

    // Remove existing items in media flex
    mediaAreaFlexBox.items.clear();

    if (getNumOverheadLabels() > 0)
    {
        // Add overhead panel if there are labels to display
        mediaAreaFlexBox.items.add(FlexItem(overheadPanel)
                                       .withHeight(labelHeight + 2 * controlSpacing)
                                       .withMargin({ 0,
                                                     getVerticalControlsWidth(),
                                                     static_cast<float>(controlSpacing),
                                                     getMediaXPos() }));
    }

    // Media component takes remaining space
    mediaAreaFlexBox.items.add(FlexItem(mediaComponent).withFlex(1));

    if (! isThumbnailTrack() & horizontalScrollBar.isVisible())
    {
        // Add horizontal scrollbar with fixed height
        mediaAreaFlexBox.items.add(FlexItem(horizontalScrollBar)
                                       .withHeight(scrollBarSize + 2 * controlSpacing)
                                       .withMargin({ static_cast<float>(controlSpacing),
                                                     getVerticalControlsWidth(),
                                                     0,
                                                     getMediaXPos() }));
    }

    // Perform layout in media area
    mediaAreaFlexBox.performLayout(mediaAreaContainer.getLocalBounds());

    repositionLabels();
}

void MediaDisplayComponent::repositionLabels() //
{
    if (! isFileLoaded())
    {
        return;
    }

    float mediaWidth = getMediaWidth();
    float mediaHeight = getMediaHeight();

    float pixelsPerSecond = mediaWidth / static_cast<float>(visibleRange.getLength());

    float minLabelWidth = 0.1f * mediaWidth;
    float maxLabelWidth = 0.1f * pixelsPerSecond;

    float contentWidth = static_cast<float>(mediaWidth);
    float minVisibilityWidth = contentWidth / 200.0f;
    float maxVisibilityWidth = contentWidth / 3.0f;

    minLabelWidth = jmin(minLabelWidth, maxVisibilityWidth);
    maxLabelWidth = jmax(maxLabelWidth, minVisibilityWidth);

    auto positionLabels = [this, minLabelWidth, maxLabelWidth, mediaHeight](auto& labels)
    {
        for (auto l : labels)
        {
            if (l == nullptr)
                continue;

            float labelWidth = jmax(
                minLabelWidth,
                jmin(maxLabelWidth, l->getTextWidth() + 2.0f * static_cast<float>(textSpacing)));

            float labelStartTime = static_cast<float>(l->getTime());
            float labelStopTime = labelStartTime + static_cast<float>(l->getDuration());

            float xPos = correctToBounds(timeToMediaX(labelStartTime + l->getDuration() / 2.0f)
                                             - labelWidth / 2.0f,
                                         labelWidth);
            float yPos = 1.0f;

            if (auto lo = dynamic_cast<LabelOverlayComponent*>(l))
            {
                yPos = lo->getRelativeY() * mediaHeight;
                yPos -= static_cast<float>(labelHeight) / 2.0f;
                yPos = jmin(mediaHeight - static_cast<float>(labelHeight), jmax(0.0f, yPos));
                yPos = mediaYToDisplayY(yPos);
            }
            Rectangle<float> labelBounds(xPos, yPos, labelWidth, static_cast<float>(labelHeight));
            l->setBounds(labelBounds.toNearestInt());
            l->toFront(true);

            float leftLabelMarkerPos = static_cast<float>(
                correctToBounds(timeToMediaX(labelStartTime), cursorWidth / 2.0f));
            l->setLeftMarkerBounds(
                Rectangle<float>(leftLabelMarkerPos, 0, cursorWidth, mediaHeight).toNearestInt());

            float rightLabelMarkerPos = static_cast<float>(
                correctToBounds(timeToMediaX(labelStopTime), cursorWidth / 2.0f));
            l->setRightMarkerBounds(
                Rectangle<float>(rightLabelMarkerPos, 0, cursorWidth, mediaHeight).toNearestInt());

            float durationWidth =
                jmax(0.0f, rightLabelMarkerPos - leftLabelMarkerPos - cursorWidth / 2.0f);
            l->setDurationFillBounds(
                Rectangle<float>(
                    leftLabelMarkerPos + cursorWidth / 2, 0, durationWidth, mediaHeight)
                    .toNearestInt());

            if (l->getIndex() == currentTempFileIdx)
            {
                l->setVisible(true);
            }
            else
            {
                l->setVisible(false);
            }
        }
    };

    positionLabels(overheadLabels);
    positionLabels(labelOverlays);
}

void MediaDisplayComponent::resetDisplay()
{
    clearLabels();
    resetMedia();
    resetPaths();
    resetScrollBar();
    resetButtonState();
    //sendChangeMessage();
}

void MediaDisplayComponent::resetPaths()
{
    originalFilePath = URL();

    tempFilePaths.clear();
    currentTempFileIdx = -1;
}

void MediaDisplayComponent::resetScrollBar()
{
    horizontalZoomFactor = 1.0;
    horizontalScrollBar.setRangeLimits({ 0.0, 1.0 });
    horizontalScrollBar.setVisible(false);
}

void MediaDisplayComponent::resetButtonState()
{
    playStopButton.setMode(playButtonInactiveInfo.label);
    chooseFileButton.setMode(chooseButtonInfo.label);
    saveFileButton.setMode(saveButtonInactiveInfo.label);
}

void MediaDisplayComponent::resetTransport()
{
    transportSource.stop();
    transportSource.setSource(nullptr);
}

void MediaDisplayComponent::initializeDisplay(const URL& filePath)
{
    resetDisplay();

    setOriginalFilePath(filePath);
    updateDisplay(filePath);

    horizontalScrollBar.setVisible(true);
    updateVisibleRange({ 0.0, getTotalLengthInSecs() });
    resized(); // Needed to display scrollbar after loading
}

void MediaDisplayComponent::updateDisplay(const URL& filePath)
{
    resetMedia();

    loadMediaFile(filePath);
    postLoadActions(filePath);

    currentPositionMarker.toFront(true);

    Range<double> range(0.0, getTotalLengthInSecs());

    horizontalScrollBar.setRangeLimits(range);

    playStopButton.setMode(playButtonActiveInfo.label);
    saveFileButton.setMode(saveButtonActiveInfo.label);
}

void MediaDisplayComponent::setOriginalFilePath(URL filePath)
{
    originalFilePath = filePath;

    addNewTempFile();
}

void MediaDisplayComponent::addNewTempFile()
{
    // Prune any future files in chain before adding new temp file
    clearFutureTempFiles();

    int numTempFiles = tempFilePaths.size();

    // Obtain original file used to initialize display
    File originalFile = originalFilePath.getLocalFile();

    File targetFile; // File to copy to new temp file

    if (! numTempFiles)
    {
        // Copy original file if no temp files
        targetFile = originalFile;
    }
    else
    {
        // Otherwise copy most recent temp file
        targetFile = getTempFilePath().getLocalFile();
    }

    String tempDirectory =
        File::getSpecialLocation(File::SpecialLocationType::tempDirectory).getFullPathName();

    String targetFileName = originalFile.getFileNameWithoutExtension();
    String targetFileExtension = originalFile.getFileExtension();

    URL tempFilePath = URL(File(tempDirectory + "/HARP/" + targetFileName + "_"
                                + String(numTempFiles) + targetFileExtension));

    File tempFile = tempFilePath.getLocalFile();

    tempFile.getParentDirectory().createDirectory();

    if (! targetFile.copyFileTo(tempFile))
    {
        DBG("MediaDisplayComponent::addNewTempFile: Failed to copy file "
            << targetFile.getFullPathName() << " to " << tempFile.getFullPathName() << ".");
    }
    else
    {
        DBG("MediaDisplayComponent::addNewTempFile: Copied file "
            << targetFile.getFullPathName() << " to " << tempFile.getFullPathName() << ".");
    }

    tempFilePaths.add(tempFilePath);
    currentTempFileIdx++;
}

bool MediaDisplayComponent::iteratePreviousTempFile()
{
    if (currentTempFileIdx > 0)
    {
        currentTempFileIdx--;

        updateDisplay(getTempFilePath());

        return true;
    }
    else
    {
        return false;
    }
}

bool MediaDisplayComponent::iterateNextTempFile()
{
    if (currentTempFileIdx + 1 < tempFilePaths.size())
    {
        currentTempFileIdx++;

        updateDisplay(getTempFilePath());

        return true;
    }
    else
    {
        return false;
    }
}

void MediaDisplayComponent::clearFutureTempFiles()
{
    int n = tempFilePaths.size() - (currentTempFileIdx + 1);

    tempFilePaths.removeLast(n);

    clearLabels(currentTempFileIdx + 1);
}

void MediaDisplayComponent::overwriteOriginalFile()
{
    File targetFile = originalFilePath.getLocalFile();
    File tempFile = getTempFilePath().getLocalFile();

    String parentDirectory = targetFile.getParentDirectory().getFullPathName();
    String targetFileName = targetFile.getFileNameWithoutExtension();
    String targetFileExtension = targetFile.getFileExtension();

    File backupFile =
        File(parentDirectory + "/" + targetFileName + "_BACKUP" + targetFileExtension);

    if (targetFile.copyFileTo(backupFile))
    {
        DBG("MediaDisplayComponent::overwriteOriginalFile: Created backup of file"
            << targetFile.getFullPathName() << " at " << backupFile.getFullPathName() << ".");
    }
    else
    {
        DBG("MediaDisplayComponent::overwriteOriginalFile: Failed to create backup of file"
            << targetFile.getFullPathName() << " at " << backupFile.getFullPathName() << ".");
    }

    if (tempFile.copyFileTo(targetFile))
    {
        DBG("MediaDisplayComponent::overwriteOriginalFile: Overwriting file "
            << targetFile.getFullPathName() << " with " << tempFile.getFullPathName() << ".");
    }
    else
    {
        DBG("MediaDisplayComponent::overwriteOriginalFile: Failed to overwrite file "
            << targetFile.getFullPathName() << " with " << tempFile.getFullPathName() << ".");
    }
}

void MediaDisplayComponent::filesDropped(const StringArray& files, int /*x*/, int /*y*/)
{
    for (int i = 1; i < files.size(); i++)
    {
        DBG("MediaDisplayComponent::filesDropped: Ignoring additional file " << files[i] << ".");
    }

    File mediaFile = File(files[0]);

    if (getOriginalFilePath() == URL(mediaFile)
        || (isFileLoaded() && getTempFilePath() == URL(mediaFile)))
    {
        DBG("MediaDisplayComponent::filesDropped: Ignoring self-drag.");
        return;
    }

    if (! getInstanceExtensions().contains(mediaFile.getFileExtension()))
    {
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                         "Invalid File",
                                         "This display supports the following file types: "
                                             + getInstanceExtensions().joinIntoString(", ") + ".",
                                         "OK");
    }
    else
    {
        initializeDisplay(URL(mediaFile));
    }
}

void MediaDisplayComponent::chooseFileCallback()
{
    StringArray validExtensions = StringArray(getInstanceExtensions());
    String filePatternsAllowed = "*" + validExtensions.joinIntoString(";*");

    chooseFileBrowser =
        std::make_unique<FileChooser>("Select a media file...", File(), filePatternsAllowed);

    chooseFileBrowser->launchAsync(FileBrowserComponent::openMode
                                       | FileBrowserComponent::canSelectFiles,
                                   [this](const FileChooser& fc)
                                   {
                                       File chosenFile = fc.getResult();
                                       if (chosenFile != File {})
                                       {
                                           initializeDisplay(URL(chosenFile));
                                       }
                                   });
}

void MediaDisplayComponent::saveFileCallback()
{
    if (saveFileButton.getModeName() == saveButtonActiveInfo.label)
    {
        overwriteOriginalFile();
        saveFileButton.setMode(saveButtonInactiveInfo.label);
        statusBox->setStatusMessage("File saved successfully");
    }
}

float MediaDisplayComponent::getPixelsPerSecond()
{
    if (visibleRange.getLength())
    {
        return getMediaWidth() / static_cast<float>(visibleRange.getLength());
    }
    else
    {
        return 0.0f;
    }
}

double MediaDisplayComponent::mediaXToTime(const float x)
{
    float x_ = jmin(getMediaWidth(), jmax(0.0f, x));

    if (visibleRange.getLength())
    {
        return static_cast<double>(x_ / getPixelsPerSecond()) + getTimeAtOrigin();
    }
    else
    {
        return 0.0;
    }
}

float MediaDisplayComponent::timeToMediaX(const double t)
{
    double t_ = jmin(getTotalLengthInSecs(), jmax(0.0, t));

    if (visibleRange.getLength())
    {
        return static_cast<float>(t_ - getTimeAtOrigin()) * getPixelsPerSecond();
    }
    else
    {
        return 0.0f;
    }
}

float MediaDisplayComponent::mediaXToDisplayX(const float mX)
{
    float offsetX = 0;
    float visibleStartX = 0;

    if (visibleRange.getLength())
    {
        offsetX = static_cast<float>(getTimeAtOrigin()) * getPixelsPerSecond();
        visibleStartX = static_cast<float>(visibleRange.getStart() * getPixelsPerSecond());
    }

    float dX = static_cast<float>(controlSpacing) + getMediaXPos() + mX - (visibleStartX - offsetX);

    return dX;
}

void MediaDisplayComponent::mouseDrag(const MouseEvent& e)
{
    if (e.eventComponent == getMediaComponent() && isFileLoaded())
    {
        if (! isPlaying() && getLocalBounds().contains(getMouseXYRelative()))
        {
            float x_ = static_cast<float>(e.x);

            double visibleStart = visibleRange.getStart();
            double visibleStop = visibleStart + visibleRange.getLength();

            x_ = jmax(timeToMediaX(visibleStart), x_);
            x_ = jmin(timeToMediaX(visibleStop), x_);

            setPlaybackPosition(mediaXToTime(x_));
        }

        if (! getLocalBounds().contains(getMouseXYRelative()))
        {
            performExternalDragDropOfFiles(
                StringArray(getTempFilePath().getLocalFile().getFullPathName()), true, this);

            if (! isPlaying())
            {
                setPlaybackPosition(0.0);
            }
        }

        updateCursorPosition();
    }
}

void MediaDisplayComponent::mouseUp(const MouseEvent& e)
{
    mouseDrag(e); // Make sure playback position has been updated

    if (e.eventComponent == getMediaComponent() && isFileLoaded()
        && isMouseOver(true)) // Only start playback if still within this area
    {
        start();
        //sendChangeMessage();
    }
}

void MediaDisplayComponent::start()
{
    startPlaying();

    startTimerHz(40);

    playStopButton.setMode(stopButtonInfo.label);
}

void MediaDisplayComponent::stop()
{
    stopPlaying();

    stopTimer();

    currentPositionMarker.setVisible(false);
    setPlaybackPosition(0.0);

    playStopButton.setMode(playButtonActiveInfo.label);
}

void MediaDisplayComponent::updateVisibleRange(Range<double> r)
{
    visibleRange = r;

    horizontalScrollBar.setCurrentRange(visibleRange);
    updateCursorPosition();
    repositionLabels();
    repaint();
}

String MediaDisplayComponent::getMediaHandlerInstructions()
{
    String toolTipText = mediaHandlerInstructions;

    for (OverheadLabelComponent* label : overheadLabels)
    {
        if (label->isMouseOver())
        {
            toolTipText = label->getDescription();
        }
    }

    for (LabelOverlayComponent* label : labelOverlays)
    {
        if (label->isMouseOver())
        {
            toolTipText = label->getDescription();
        }
    }

    return toolTipText;
}

void MediaDisplayComponent::setMediaHandlerInstructions(String instructions)
{
    mediaHandlerInstructions = instructions;
}

void MediaDisplayComponent::addLabels(LabelList& labels)
{
    for (const auto& l : labels)
    {
        if (! shouldRenderLabel(l))
        {
            continue;
        }
        std::unique_ptr<OutputLabelComponent> lc =
            std::make_unique<OutputLabelComponent>((double) l->t, l->label);
        ;

        if ((l->description).has_value())
        {
            lc->setDescription((l->description).value());
        }

        if ((l->duration).has_value())
        {
            lc->setDuration((double) (l->duration).value());
        }

        if ((l->color).has_value())
        {
            lc->setColor(Colour(static_cast<uint32_t>((l->color).value())));
        }

        if ((l->link).has_value())
        {
            lc->setLink((l->link).value());
        }

        float y = 0.0f;

        bool isOverlay = false;

        if (auto audioLabel = dynamic_cast<AudioLabel*>(l.get()))
        {
            if ((audioLabel->amplitude).has_value())
            {
                isOverlay = true;

                float amp = (audioLabel->amplitude).value();

                y = LabelOverlayComponent::amplitudeToRelativeY(amp);
            }
        }

        if (auto midiLabel = dynamic_cast<MidiLabel*>(l.get()))
        {
            if ((midiLabel->pitch).has_value())
            {
                isOverlay = true;

                float p = (midiLabel->pitch).value();

                y = LabelOverlayComponent::pitchToRelativeY(p);
            }
        }

        if (isOverlay)
        {
            auto* lo = new LabelOverlayComponent(*static_cast<LabelOverlayComponent*>(lc.get()));
            lo->setRelativeY(y);

            addLabelOverlay(lo);
        }
        else
        {
            auto* ol = new OverheadLabelComponent(*static_cast<OverheadLabelComponent*>(lc.get()));
            addOverheadLabel(ol);
        }
    }
}

void MediaDisplayComponent::addLabelOverlay(LabelOverlayComponent* l)
{
    l->setFont(Font(jmax(minFontSize, labelHeight - 2 * textSpacing)));
    l->setIndex(currentTempFileIdx);
    labelOverlays.add(l);

    Component* mediaComponentPtr = getMediaComponent();
    mediaComponentPtr->addAndMakeVisible(l);
    l->addMarkersTo(mediaComponentPtr);

    // mediaComponent.addAndMakeVisible(l);
    // l->addMarkersTo(mediaComponent);

    repositionLabels();
}

void MediaDisplayComponent::addOverheadLabel(OverheadLabelComponent* l)
{
    l->setFont(Font(jmax(minFontSize, labelHeight - 2 * textSpacing)));
    l->setIndex(currentTempFileIdx);
    overheadLabels.add(l);

    overheadPanel.addAndMakeVisible(l);

    Component* mediaComponentPtr = getMediaComponent();
    l->addMarkersTo(mediaComponentPtr);

    resized();
}

void MediaDisplayComponent::clearLabels(int processingIdxCutoff)
{
    for (int i = labelOverlays.size() - 1; i >= 0; --i)
    {
        LabelOverlayComponent* l = labelOverlays[i];

        if (l->getIndex() >= processingIdxCutoff)
        {
            removeLabelOverlay(l);
        }
    }

    if (! processingIdxCutoff)
    {
        labelOverlays.clear();
    }

    for (int i = overheadLabels.size() - 1; i >= 0; --i)
    {
        OverheadLabelComponent* l = overheadLabels[i];

        if (l->getIndex() >= processingIdxCutoff)
        {
            removeOverheadLabel(l);
        }
    }

    if (! processingIdxCutoff)
    {
        overheadLabels.clear();
    }

    resized();
}

void MediaDisplayComponent::removeLabelOverlay(LabelOverlayComponent* l)
{
    Component* mediaComponentPtr = getMediaComponent();

    l->removeMarkersFrom(mediaComponentPtr);
    mediaComponentPtr->removeChildComponent(l);
    labelOverlays.removeObject(l);
}

void MediaDisplayComponent::removeOverheadLabel(OverheadLabelComponent* l)
{
    Component* mediaComponentPtr = getMediaComponent();

    l->removeMarkersFrom(mediaComponentPtr);
    overheadPanel.removeChildComponent(l);

    overheadLabels.removeObject(l);
}

int MediaDisplayComponent::getNumOverheadLabels()
{
    int nOverheadLabels = 0;

    for (auto l : overheadLabels)
    {
        if (l->getIndex() == currentTempFileIdx)
        {
            nOverheadLabels++;
        }
    }

    return nOverheadLabels;
}

void MediaDisplayComponent::horizontalMove(float deltaX)
{
    auto totalLength = visibleRange.getLength();
    auto visibleStart = visibleRange.getStart();
    // auto scrollTime = mediaXToTime(evt.position.getX());
    auto newStart = visibleStart - deltaX * totalLength / 10.0;
    newStart = jlimit(0.0, jmax(0.0, getTotalLengthInSecs() - totalLength), newStart);

    updateVisibleRange({ newStart, newStart + totalLength });
}

void MediaDisplayComponent::horizontalZoom(float deltaZoom, float scrollPosX)
{
    auto totalLength = visibleRange.getLength();
    auto visibleStart = visibleRange.getStart();
    // auto scrollTime = mediaXToTime(evt.position.getX());
    horizontalZoomFactor = jlimit(1.0, 1.99, horizontalZoomFactor + deltaZoom);

    auto newScale = jmax(0.01, getTotalLengthInSecs() * (2 - horizontalZoomFactor));

    auto newStart = scrollPosX - newScale * (scrollPosX - visibleStart) / totalLength;
    auto newEnd = scrollPosX + newScale * (visibleStart + totalLength - scrollPosX) / totalLength;

    updateVisibleRange({ newStart, newEnd });
}

int MediaDisplayComponent::correctToBounds(float x, float width)
{
    x = jmax(timeToMediaX(0.0), x);
    x = jmin(timeToMediaX(getTotalLengthInSecs()) - width, x);

    return static_cast<int>(x);
}

// TODO - may be able to simplify some of this logic by embedding cursor in media component
void MediaDisplayComponent::updateCursorPosition()
{
    bool displayCursor = isFileLoaded()
                         && (isPlaying()
                             || (getMediaComponent()->isMouseButtonDown(false)
                                 && getLocalBounds().contains(getMouseXYRelative())));

    float cursorPositionX = mediaXToDisplayX(timeToMediaX(getPlaybackPosition()));

    Rectangle<int> mediaBounds = mediaComponent.getBounds();
    Rectangle<int> mediaAreaBounds = mediaAreaContainer.getBounds();

    float cursorBoundsStartX = static_cast<float>(mediaBounds.getX()) + getMediaXPos();
    float cursorBoundsWidth = static_cast<float>(visibleRange.getLength() * getPixelsPerSecond());

    // TODO - due to very small differences, cursor may not be visible at media bounds when zoomed in
    if (cursorPositionX >= cursorBoundsStartX
        && cursorPositionX <= (cursorBoundsStartX + cursorBoundsWidth))
    {
        currentPositionMarker.setVisible(displayCursor);
    }
    else
    {
        currentPositionMarker.setVisible(false);
    }

    cursorPositionX -= cursorWidth / 2.0f;
    cursorPositionX += static_cast<float>(mediaAreaBounds.getX());

    currentPositionMarker.setRectangle(
        Rectangle<float>(cursorPositionX,
                         static_cast<float>(mediaBounds.getY()),
                         cursorWidth,
                         static_cast<float>(mediaBounds.getHeight())));
}

void MediaDisplayComponent::timerCallback()
{
    if (isPlaying())
    {
        updateVisibleRange(visibleRange);
    }
    else
    {
        stop();
        //sendChangeMessage();
    }
}

void MediaDisplayComponent::scrollBarMoved(ScrollBar* scrollBarThatHasMoved,
                                           double scrollBarRangeStart)
{
    if (scrollBarThatHasMoved == &horizontalScrollBar)
    {
        updateVisibleRange(visibleRange.movedToStartAt(scrollBarRangeStart));
    }
}

void MediaDisplayComponent::mouseWheelMove(const MouseEvent& evt, const MouseWheelDetails& wheel)
{
    if (getTotalLengthInSecs() > 0.0)
    {
#if (JUCE_MAC)
        bool commandMod = evt.mods.isCommandDown();
#else
        bool commandMod = evt.mods.isCtrlDown();
#endif
        bool shiftMod = evt.mods.isShiftDown();

        auto scrollTime = static_cast<float>(mediaXToTime(evt.position.getX()));

        if (! commandMod)
        {
            if (std::abs(wheel.deltaX) > 2 * std::abs(wheel.deltaY))
            {
                // Horizontal scroll when using 2-finger swipe in macbook trackpad
                horizontalMove(wheel.deltaX);
            }
            else if (std::abs(wheel.deltaY) > 2 * std::abs(wheel.deltaX))
            {
                if (shiftMod)
                {
                    horizontalMove(wheel.deltaY);
                }
                else
                {
                    horizontalZoom(wheel.deltaY, scrollTime);
                }
            }
            else
            {
                // Do nothing
            }
        }

        repaint();
    }
}

void MediaDisplayComponent::mouseEnter(const MouseEvent& /*event*/)
{
    if (instructionBoxWriter)
        // instructionBoxWriter(mediaHandlerInstructions);
        instructionBoxWriter(getMediaHandlerInstructions());
}
void MediaDisplayComponent::mouseExit(const MouseEvent& /*event*/)
{
    if (instructionBoxWriter)
        instructionBoxWriter("");
}
