#include "MediaDisplayComponent.h"

MediaDisplayComponent::MediaDisplayComponent() : MediaDisplayComponent("Media Track") {}

MediaDisplayComponent::MediaDisplayComponent(String name, bool req)
    : trackName(name), required(req)
{
    formatManager.registerBasicFormats();

    deviceManager.initialise(0, 2, nullptr, true, {}, nullptr);
    deviceManager.addAudioCallback(&sourcePlayer);

    sourcePlayer.setSource(&transportSource);

    trackNameLabel.setText(trackName, juce::dontSendNotification);
    headerComponent.addAndMakeVisible(trackNameLabel);
    populateTrackHeader();
    addAndMakeVisible(headerComponent);

    horizontalScrollBar.setAutoHide(false);
    horizontalScrollBar.addListener(this);

    mediaAreaContainer.addMouseListener(this, true);
    mediaAreaContainer.addAndMakeVisible(overheadPanel);
    mediaAreaContainer.addAndMakeVisible(mediaComponent);
    mediaAreaContainer.addAndMakeVisible(horizontalScrollBar);
    addAndMakeVisible(mediaAreaContainer);

    currentPositionMarker.setFill(Colours::white.withAlpha(0.85f));
    addAndMakeVisible(currentPositionMarker);

    resetPaths();
    resetScrollBar();
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
    auto totalBounds = getLocalBounds();

    // Build trackRowBox items
    mainFlexBox.items.clear();
    mainFlexBox.items.add(
        juce::FlexItem(headerComponent).withFlex(1).withMaxWidth(40).withMargin(4));
    // Media area takes remaining space
    // mainFlexBox.items.add(juce::FlexItem(mediaComponent).withFlex(8));
    mainFlexBox.items.add(juce::FlexItem(mediaAreaContainer).withFlex(8));

    mainFlexBox.performLayout(totalBounds);

    // Set up the vertical layout within the media area container
    mediaAreaFlexBox.flexDirection = juce::FlexBox::Direction::column;
    mediaAreaFlexBox.items.clear();

    // Add overhead panel if needed
    if (getNumOverheadLabels() > 0)
    {
        mediaAreaFlexBox.items.add(juce::FlexItem(overheadPanel)
                                       .withHeight(labelHeight + 2 * controlSpacing)
                                       .withMargin({ 0,
                                                     getVerticalControlsWidth(),
                                                     static_cast<float>(controlSpacing),
                                                     getMediaXPos() }));
    }

    // Media component takes remaining space
    mediaAreaFlexBox.items.add(juce::FlexItem(mediaComponent).withFlex(1));

    // Add horizontal scrollbar with fixed height
    mediaAreaFlexBox.items.add(juce::FlexItem(horizontalScrollBar)
                                   .withHeight(scrollBarSize + 2 * controlSpacing)
                                   .withMargin({ static_cast<float>(controlSpacing),
                                                 getVerticalControlsWidth(),
                                                 0,
                                                 getMediaXPos() }));

    // Perform layout in media area
    mediaAreaFlexBox.performLayout(mediaAreaContainer.getLocalBounds());

    // Set up headerFlexBox for the controls inside headerComponent
    headerFlexBox.flexDirection = juce::FlexBox::Direction::row;
    headerFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    headerFlexBox.alignItems = juce::FlexBox::AlignItems::stretch;

    juce::FlexBox buttonsFlexBox;
    buttonsFlexBox.flexDirection = juce::FlexBox::Direction::column;
    buttonsFlexBox.justifyContent = juce::FlexBox::JustifyContent::center;
    buttonsFlexBox.alignItems = juce::FlexBox::AlignItems::stretch;
    buttonsFlexBox.items.add(juce::FlexItem(playStopButton).withHeight(30));
    buttonsFlexBox.items.add(juce::FlexItem(chooseFileButton).withHeight(30));
    buttonsFlexBox.items.add(juce::FlexItem(saveFileButton).withHeight(30));

    headerFlexBox.items.clear();
    headerFlexBox.items.add(juce::FlexItem(trackNameLabel).withFlex(1));
    headerFlexBox.items.add(juce::FlexItem(buttonsFlexBox).withFlex(1));

    // Perform layout of controls inside headerComponent
    headerFlexBox.performLayout(headerComponent.getLocalBounds());

    // After layout, adjust the trackNameLabel
    auto labelBounds = trackNameLabel.getBounds().toFloat();
    auto labelCentre = labelBounds.getCentre();

    // Apply rotation
    trackNameLabel.setTransform(juce::AffineTransform::rotation(
        -juce::MathConstants<float>::halfPi, labelCentre.x, labelCentre.y));

    // Swap width and height
    float newWidth = labelBounds.getHeight();
    float newHeight = labelBounds.getWidth();

    // Update label bounds and position
    trackNameLabel.setBounds(labelBounds.withSize(newWidth, newHeight).toNearestInt());

    // Reposition the label to center it within headerComponent
    auto headerBounds = headerComponent.getLocalBounds();
    float labelX = (static_cast<float>(headerBounds.getWidth()) - newWidth) / 2.0f;
    float labelY = (static_cast<float>(headerBounds.getHeight()) - newHeight) / 2.0f;
    trackNameLabel.setTopLeftPosition(static_cast<int>(labelX), static_cast<int>(labelY));

    // Set text justification to centered
    trackNameLabel.setJustificationType(juce::Justification::centred);

    repositionLabels();
}

void MediaDisplayComponent::repositionLabels()
{
    if (visibleRange.getLength() == 0.0)
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
            juce::Rectangle<float> labelBounds(
                xPos, yPos, labelWidth, static_cast<float>(labelHeight));
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
    sendChangeMessage();
}

void MediaDisplayComponent::initializeDisplay(const URL& filePath)
{
    resetDisplay();

    setNewTarget(filePath);
    updateDisplay(filePath);

    horizontalScrollBar.setVisible(true);
    updateVisibleRange({ 0.0, getTotalLengthInSecs() });
}

void MediaDisplayComponent::updateDisplay(const URL& filePath)
{
    resetMedia();

    loadMediaFile(filePath);
    postLoadActions(filePath);

    currentPositionMarker.toFront(true);

    Range<double> range(0.0, getTotalLengthInSecs());

    horizontalScrollBar.setRangeLimits(range);
}

void MediaDisplayComponent::addNewTempFile()
{ // either just before processing by processCallback, or when importing a new file
    clearFutureTempFiles();

    int numTempFiles = tempFilePaths.size();
    // TODO: for outputMediaDisplays there might not be a
    // targetFilePath yet, so we should handle that case
    // "originalFile" is the first file that was displayed
    // in this mediaDisplay (either the first input)
    // or the first output)
    // File originalFile;
    // if (currentTempFileIdx == 0)
    // {
    //     originalFile = targetFilePath.getLocalFile();
    // }
    // else
    // {
    //     originalFile = tempFilePaths.getReference(currentTempFileIdx - 1).getLocalFile();
    // }

    // targetFilePath is of type URL
    // for outputMediaDisplays, there isn't a targetFilePath.
    // if (targetFilePath.isEmpty())
    // {
    //     DBG("MediaDisplayComponent::addNewTempFile: No target file path.");
    //     return;
    // }
    // there might be past tempFiles
    File originalFile = targetFilePath.getLocalFile();
    // in case of new import file, targetFilePath is already the new file, so the originalFile is always the latest imported file, and not the first imported file
    File targetFile;
    // targetFile is not always the targetFilePath.
    if (! numTempFiles) // if zero past files.
    { // when no temp files exist, the only available file is the original file which is the original targetFilePath
        targetFile = originalFile;
    }
    else
    { // else we create a tempFile from the latest tempFile
        targetFile = getTempFilePath().getLocalFile();
    }
    // the targetFile is the one we are going to create a new tempFile from
    String docsDirectory =
        File::getSpecialLocation(File::SpecialLocationType::userDocumentsDirectory)
            .getFullPathName();

    String targetFileName = originalFile.getFileNameWithoutExtension();
    String targetFileExtension = originalFile.getFileExtension();

    URL tempFilePath = URL(File(docsDirectory + "/HARP/" + targetFileName + "_"
                                + String(numTempFiles) + targetFileExtension));

    File tempFile = tempFilePath.getLocalFile();

    tempFile.getParentDirectory().createDirectory();

    if (! targetFile.copyFileTo(tempFile))
    {
        DBG("MediaDisplayComponent::generateTempFile: Failed to copy file "
            << targetFile.getFullPathName() << " to " << tempFile.getFullPathName() << ".");

        AlertWindow(
            "Error", "Failed to create temporary file for processing.", AlertWindow::WarningIcon);
    }
    else
    {
        DBG("MediaDisplayComponent::generateTempFile: Copied file "
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

void MediaDisplayComponent::overwriteTarget()
{
    // Overwrite the original file - necessary for seamless sample editing integration

    File targetFile = targetFilePath.getLocalFile();
    File tempFile = getTempFilePath().getLocalFile();

    String parentDirectory = targetFile.getParentDirectory().getFullPathName();
    String targetFileName = targetFile.getFileNameWithoutExtension();
    String targetFileExtension = targetFile.getFileExtension();

    File backupFile =
        File(parentDirectory + "/" + targetFileName + "_BACKUP" + targetFileExtension);

    if (targetFile.copyFileTo(backupFile))
    {
        DBG("MediaDisplayComponent::overwriteTarget: Created backup of file"
            << targetFile.getFullPathName() << " at " << backupFile.getFullPathName() << ".");
    }
    else
    {
        DBG("MediaDisplayComponent::overwriteTarget: Failed to create backup of file"
            << targetFile.getFullPathName() << " at " << backupFile.getFullPathName() << ".");
    }

    if (tempFile.copyFileTo(targetFile))
    {
        DBG("MediaDisplayComponent::overwriteTarget: Overwriting file "
            << targetFile.getFullPathName() << " with " << tempFile.getFullPathName() << ".");
    }
    else
    {
        DBG("MediaDisplayComponent::overwriteTarget: Failed to overwrite file "
            << targetFile.getFullPathName() << " with " << tempFile.getFullPathName() << ".");
    }
}

void MediaDisplayComponent::filesDropped(const StringArray& files, int /*x*/, int /*y*/)
{
    // TODO - warning or handling for additional files

    // Avoid self-dragging
    if (getTargetFilePath() == URL(File(files[0])))
    {
        DBG("Won't self drag");
        return;
    }

    droppedFilePath = URL(File(files[0]));
    auto mediaFile = droppedFilePath.getLocalFile();
    // sendChangeMessage();
    // loadMediaFile(droppedFilePath);
    String extension = mediaFile.getFileExtension();

    bool matchingDisplay = getInstanceExtensions().contains(extension);

    if (! matchingDisplay)
    {
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                         "Wrong file extension",
                                         "Please drop one of the following file types: "
                                             + getInstanceExtensions().joinIntoString(", "),
                                         "OK");
    }
    else
    {
        initializeDisplay(URL(mediaFile));
        saveFileButton.setMode(saveButtonActiveInfo.label);
    }
    droppedFilePath = URL();
}

void MediaDisplayComponent::openFileChooser()
{
    StringArray allExtensions = StringArray(getInstanceExtensions());
    // allExtensions.mergeArray(midiExtensions);

    String filePatternsAllowed = "*" + allExtensions.joinIntoString(";*");

    openFileBrowser =
        std::make_unique<FileChooser>("Select a media file...", File(), filePatternsAllowed);

    openFileBrowser->launchAsync(FileBrowserComponent::openMode
                                     | FileBrowserComponent::canSelectFiles,
                                 [this](const FileChooser& browser)
                                 {
                                     File chosenFile = browser.getResult();
                                     if (chosenFile != File {})
                                     {
                                         initializeDisplay(URL(chosenFile));
                                         saveFileButton.setMode(saveButtonActiveInfo.label);
                                     }
                                 });
}

void MediaDisplayComponent::setNewTarget(URL filePath)
{
    targetFilePath = filePath;

    addNewTempFile();
}

void MediaDisplayComponent::resetTransport()
{
    transportSource.stop();
    transportSource.setSource(nullptr);
}

void MediaDisplayComponent::resetPaths()
{
    clearDroppedFile();

    targetFilePath = URL();

    tempFilePaths.clear();
    currentTempFileIdx = -1;
}

void MediaDisplayComponent::resetScrollBar()
{
    currentHorizontalZoomFactor = 1.0;
    horizontalScrollBar.setRangeLimits({ 0.0, 1.0 });
    horizontalScrollBar.setVisible(false);
}

void MediaDisplayComponent::saveCallback()
{
    if (saveFileButton.getModeName() == saveButtonActiveInfo.label)
    {
        overwriteTarget();
        // saveFileButton.setEnabled(false);
        saveFileButton.setMode(saveButtonInactiveInfo.label);
        statusBox->setStatusMessage("File saved successfully");
    }
    else
    {
        statusBox->setStatusMessage("Nothing to save");
    }
}

void MediaDisplayComponent::mouseDrag(const MouseEvent& e)
{
    if (e.eventComponent == getMediaComponent() && isFileLoaded())
    {
        if (! isPlaying() && getLocalBounds().contains(getMouseXYRelative()))
        {
            float x_ = (float) e.x;

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
    mouseDrag(e); // make sure playback position has been updated

    if (e.eventComponent == getMediaComponent() && isFileLoaded()
        && isMouseOver(true)) //Only start playback if we're still in this area
    {
        start();
        sendChangeMessage();
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

    playStopButton.setMode(playButtonInfo.label);
}

float MediaDisplayComponent::getPixelsPerSecond()
{
    return getMediaWidth() / static_cast<float>(visibleRange.getLength());
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

double MediaDisplayComponent::mediaXToTime(const float x)
{
    float x_ = jmin(getMediaWidth(), jmax(0.0f, x));

    double t = ((double) (x_ / getPixelsPerSecond())) + getTimeAtOrigin();

    return t;
}

float MediaDisplayComponent::timeToMediaX(const double t)
{
    float x;

    if (visibleRange.getLength() <= 0)
    {
        x = 0;
    }
    else
    {
        double t_ = jmin(getTotalLengthInSecs(), jmax(0.0, t));

        x = ((float) (t_ - getTimeAtOrigin())) * getPixelsPerSecond();
    }

    return x;
}

float MediaDisplayComponent::mediaXToDisplayX(const float mX)
{
    float visibleStartX = static_cast<float>(visibleRange.getStart() * getPixelsPerSecond());
    float offsetX = static_cast<float>(getTimeAtOrigin()) * getPixelsPerSecond();

    float dX = static_cast<float>(controlSpacing) + getMediaXPos() + mX - (visibleStartX - offsetX);

    return dX;
}

void MediaDisplayComponent::horizontalMove(float deltaX)
{
    auto totalLength = visibleRange.getLength();
    auto visibleStart = visibleRange.getStart();
    // auto scrollTime = mediaXToTime(evt.position.getX());
    auto newStart = visibleStart - deltaX * totalLength / 10.0;
    newStart = jlimit(0.0, jmax(0.0, getTotalLengthInSecs() - totalLength), newStart);

    if (! isPlaying())
    {
        updateVisibleRange({ newStart, newStart + totalLength });
    }
}

void MediaDisplayComponent::horizontalZoom(float deltaZoom, float scrollPosX)
{
    auto totalLength = visibleRange.getLength();
    auto visibleStart = visibleRange.getStart();
    // auto scrollTime = mediaXToTime(evt.position.getX());
    currentHorizontalZoomFactor = jlimit(1.0, 1.99, currentHorizontalZoomFactor + deltaZoom);

    auto newScale = jmax(0.01, getTotalLengthInSecs() * (2 - currentHorizontalZoomFactor));

    auto newStart = scrollPosX - newScale * (scrollPosX - visibleStart) / totalLength;
    auto newEnd = scrollPosX + newScale * (visibleStart + totalLength - scrollPosX) / totalLength;

    updateVisibleRange({ newStart, newEnd });
}

void MediaDisplayComponent::populateTrackHeader()
{
    playButtonInfo = MultiButton::Mode {
        "Play",
        [this] { start(); },
        juce::Colours::limegreen,
        "Click to start playback",
        MultiButton::DrawingMode::IconOnly,
        fontaudio::Play,
    };
    stopButtonInfo = MultiButton::Mode {
        "Stop",
        [this] { stop(); },
        Colours::orangered,
        "Click to stop playback",
        MultiButton::DrawingMode::IconOnly,
        fontaudio::Stop,
    };
    playStopButton.addMode(playButtonInfo);
    playStopButton.addMode(stopButtonInfo);
    playStopButton.setMode(playButtonInfo.label);
    playStopButton.setEnabled(true);
    headerComponent.addAndMakeVisible(playStopButton);

    chooseButtonInfo = MultiButton::Mode {
        "Choose",
        [this] { openFileChooser(); }, // chooseFile();
        juce::Colours::lightblue,
        "Click to choose a media file",
        MultiButton::DrawingMode::IconOnly,
        fontawesome::Folder,
    };
    chooseFileButton.addMode(chooseButtonInfo);
    chooseFileButton.setMode(chooseButtonInfo.label);
    headerComponent.addAndMakeVisible(chooseFileButton);

    saveButtonActiveInfo = MultiButton::Mode {
        "Save1",
        [this] { saveCallback(); }, // saveFile();
        juce::Colours::lightblue,
        "Click to save the media file",
        MultiButton::DrawingMode::IconOnly,
        fontawesome::Save,
    };
    // We can use a separate mode for the save button
    // to be used when there is nothing to save
    saveButtonInactiveInfo = MultiButton::Mode {
        "Save2", // mode labels need to be unique for the button
        [this] { saveCallback(); }, // saveFile();
        juce::Colours::lightgrey,
        "Nothing to save",
        MultiButton::DrawingMode::IconOnly,
        fontawesome::Save,
    };
    saveFileButton.addMode(saveButtonActiveInfo);
    saveFileButton.addMode(saveButtonInactiveInfo);
    saveFileButton.setMode(saveButtonInactiveInfo.label);
    headerComponent.addAndMakeVisible(saveFileButton);
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
        sendChangeMessage();
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

void MediaDisplayComponent::mouseEnter(const juce::MouseEvent& /*event*/)
{
    if (instructionBoxWriter)
        // instructionBoxWriter(mediaHandlerInstructions);
        instructionBoxWriter(getMediaHandlerInstructions());
}
void MediaDisplayComponent::mouseExit(const juce::MouseEvent& /*event*/)
{
    if (instructionBoxWriter)
        instructionBoxWriter("");
}
