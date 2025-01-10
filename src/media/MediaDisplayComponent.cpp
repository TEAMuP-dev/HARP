#include "MediaDisplayComponent.h"

MediaDisplayComponent::MediaDisplayComponent()
    : MediaDisplayComponent("Media Track")
{
}

MediaDisplayComponent::MediaDisplayComponent(String trackName)
    : trackName(trackName)
{
    resetPaths();

    formatManager.registerBasicFormats();

    deviceManager.initialise(0, 2, nullptr, true, {}, nullptr);
    deviceManager.addAudioCallback(&sourcePlayer);

    sourcePlayer.setSource(&transportSource);

    addChildComponent(horizontalScrollBar);
    horizontalScrollBar.setAutoHide(false);
    horizontalScrollBar.addListener(this);

    currentPositionMarker.setFill(Colours::white.withAlpha(0.85f));
    addAndMakeVisible(currentPositionMarker);


    trackNameLabel.setText(trackName, juce::dontSendNotification);
    addAndMakeVisible(headerComponent);
    addAndMakeVisible(mediaComponent);

    // Add controls to headerComponent
    headerComponent.addAndMakeVisible(trackNameLabel);
    populateTrackHeader();
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
    mainFlexBox.items.add(juce::FlexItem(headerComponent).withFlex(1).withMaxWidth(40).withMargin(4)); 
     // Media area takes remaining space
    mainFlexBox.items.add(juce::FlexItem(mediaComponent).withFlex(8));     

    mainFlexBox.performLayout(totalBounds);

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
    trackNameLabel.setTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                                               labelCentre.x, labelCentre.y));

    // Swap width and height
    int newWidth = labelBounds.getHeight();
    int newHeight = labelBounds.getWidth();

    // Update label bounds and position
    trackNameLabel.setBounds(labelBounds.withSize(newWidth, newHeight).toNearestInt());

    // Reposition the label to center it within headerComponent
    auto headerBounds = headerComponent.getLocalBounds();
    int labelX = (headerBounds.getWidth() - newWidth) / 2;
    int labelY = (headerBounds.getHeight() - newHeight) / 2;
    trackNameLabel.setTopLeftPosition(labelX, labelY);

    // Set text justification to centered
    trackNameLabel.setJustificationType(juce::Justification::centred);
    
    repositionScrollBar();
    repositionLabels();
}


void MediaDisplayComponent::repositionScrollBar()
{
    horizontalScrollBar.setBounds(mediaComponent.getBounds()
                                      .removeFromBottom(scrollBarSize + 2 * controlSpacing)
                                      .reduced(controlSpacing));
}

void MediaDisplayComponent::repositionOverheadLabels()
{
    // for (auto l : oveheadLabels) {}
}

void MediaDisplayComponent::repositionLabelOverlays()
{
    if (! visibleRange.getLength())
    {
        return;
    }

    float mediaHeight = getMediaHeight();
    float mediaWidth = getMediaWidth();

    float pixelsPerSecond = mediaWidth / visibleRange.getLength();

    float minLabelWidth = 0.1 * getMediaWidth();
    float maxLabelWidth = 0.10 * pixelsPerSecond;

    //cb:TODO: check if mediaComponent.getBounds() is correct
    float contentWidth = mediaComponent.getBounds().getWidth();
    float minVisibilityWidth = contentWidth / 200.0f;
    float maxVisibilityWidth = contentWidth / 3.0f;

    minLabelWidth = jmin(minLabelWidth, maxVisibilityWidth);
    maxLabelWidth = jmax(maxLabelWidth, minVisibilityWidth);

    for (auto l : labelOverlays)
    {
        float textWidth = l->getFont().getStringWidthFloat(l->getText());
        float labelWidth = jmax(minLabelWidth, jmin(maxLabelWidth, textWidth + 2 * textSpacing));

        // TODO - l->getDuration() unused

        float xPos = timeToMediaX(l->getTime());
        float yPos = l->getRelativeY() * mediaHeight;

        xPos -= labelWidth / 2.0f;
        yPos -= labelHeight / 2.0f;

        xPos = jmax(timeToMediaX(0.0), xPos);
        xPos = jmin(timeToMediaX(getTotalLengthInSecs()) - labelWidth, xPos);
        yPos = jmin(mediaHeight - labelHeight, jmax(0.0f, yPos));

        l->setBounds(xPos, yPos, labelWidth, labelHeight);
    }
}

void MediaDisplayComponent::repositionLabels()
{
    repositionOverheadLabels();
    repositionLabelOverlays();
}

void MediaDisplayComponent::changeListenerCallback(ChangeBroadcaster*)
{
    repaint();
    resized();
}

void MediaDisplayComponent::resetMedia()
{
    resetPaths();
    clearLabels();
    resetDisplay();
    sendChangeMessage(); // cb: what's the point of this ? 

    currentHorizontalZoomFactor = 1.0;
    horizontalScrollBar.setRangeLimits({ 0.0, 1.0 });
    horizontalScrollBar.setVisible(false);
}

// the function we need to call when we want to load a media file
void MediaDisplayComponent::setupDisplay(const URL& filePath)
{
    resetMedia();

    setNewTarget(filePath);
    updateDisplay(filePath);

    horizontalScrollBar.setVisible(true);
    updateVisibleRange({ 0.0, getTotalLengthInSecs() });
}

void MediaDisplayComponent::updateDisplay(const URL& filePath)
{
    resetDisplay();

    loadMediaFile(filePath);
    postLoadActions(filePath);

    currentPositionMarker.toFront(true);

    Range<double> range(0.0, getTotalLengthInSecs());

    horizontalScrollBar.setRangeLimits(range);
}

void MediaDisplayComponent::addNewTempFile()
{
    clearFutureTempFiles();

    int numTempFiles = tempFilePaths.size();
    // TODO: for outputMediaDisplays there might not be a
    // targetFilePath yet, so we should handle that case
    // "originalFile" is the first file that was displayed 
    // in this mediaDisplay (either the first input)
    // or the first output)
    File originalFile;
    if (targetFilePath.isLocalFile())
        originalFile = targetFilePath.getLocalFile();        

    File targetFile;

    if (! numTempFiles)
    {
        targetFile = originalFile;
    }
    else
    {
        targetFile = getTempFilePath().getLocalFile();
    }

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

    droppedFilePath = URL(File(files[0]));
    auto mediaFile = droppedFilePath.getLocalFile();
    // sendChangeMessage();
    // loadMediaFile(droppedFilePath);
    String extension = mediaFile.getFileExtension();

    bool matchingDisplay = getInstanceExtensions().contains(extension);

    if (! matchingDisplay)
    {
        AlertWindow::showMessageBoxAsync(
            AlertWindow::WarningIcon, "Wrong file extension", "Please drop one of the following file types: " + getInstanceExtensions().joinIntoString(", "), "OK");
    }
    else
    {
        setupDisplay(URL(mediaFile));
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
                                            setupDisplay(URL(chosenFile));
                                            saveFileButton.setMode(saveButtonActiveInfo.label);
                                        }
                                    });
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
    if (e.eventComponent == getMediaComponent() && ! isPlaying())
    {
        float x_ = (float) e.x;

        double visibleStart = visibleRange.getStart();
        double visibleStop = visibleStart + visibleRange.getLength();

        x_ = jmax(timeToMediaX(visibleStart), x_);
        x_ = jmin(timeToMediaX(visibleStop), x_);

        setPlaybackPosition(mediaXToTime(x_));
        updateCursorPosition();
    }
}

void MediaDisplayComponent::mouseUp(const MouseEvent& e)
{
    mouseDrag(e); // make sure playback position has been updated

    for (OverheadLabelComponent* label : oveheadLabels)
    {
        if (label->isMouseOver()) {
            //TODO
        }
    }

    for (LabelOverlayComponent* label : labelOverlays)
    {   
        DBG("Checking label overlap");
        if (label->isMouseOver()) {
            String link = label->getLink();
            DBG("Attempting to load link " << link);
            if (link != "") {
                URL link_url = URL(link);
                if (!link_url.isWellFormed()) {
                    DBG("Link appears malformed: " << link);
                } else {
                    DBG("Opening link " << link);
                    link_url.launchInDefaultBrowser();
                    return;
                }
            }
        }
    }

    if (e.eventComponent == getMediaComponent())
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
    return getMediaWidth() / visibleRange.getLength();
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

    for (OverheadLabelComponent* label : oveheadLabels)
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

void MediaDisplayComponent::addLabels(LabelList& labels)
{
    clearLabels();

    for (const auto& l : labels)
    {
        String lbl = l->label;
        String dsc = l->description;

        if (dsc.isEmpty())
        {
            dsc = lbl;
        }

        float dur = 0.0f;

        if ((l->duration).has_value())
        {
            dur = (l->duration).value();
        }

        Colour color = Colours::purple.withAlpha(0.8f);

        if ((l->color).has_value())
        {
            color = Colour((l->color).value());
        }

        if (! dynamic_cast<AudioLabel*>(l.get()) && ! dynamic_cast<SpectrogramLabel*>(l.get())
            && ! dynamic_cast<MidiLabel*>(l.get()))
        {
            // TODO - OverheadLabelComponent((double) l->t, lbl, (double) dur, dsc, color);
        }
    }
}

void MediaDisplayComponent::addLabelOverlay(LabelOverlayComponent l)
{
    LabelOverlayComponent* label = new LabelOverlayComponent(l);
    label->setFont(Font(jmax(minFontSize, labelHeight - 2 * textSpacing)));
    labelOverlays.add(label);

    getMediaComponent()->addAndMakeVisible(label);
}

void MediaDisplayComponent::addOverheadLabel(OverheadLabelComponent l)
{
    // TODO
}

void MediaDisplayComponent::removeOutputLabel(OutputLabelComponent* l)
{
    // TODO
}

void MediaDisplayComponent::clearLabels()
{
    Component* mediaComponent = getMediaComponent();

    for (int i = 0; i < labelOverlays.size(); i++)
    {
        LabelOverlayComponent* l = labelOverlays.getReference(i);
        mediaComponent->removeChildComponent(l);

        delete l;
    }

    labelOverlays.clear();

    /*for (int i = 0; i < oveheadLabels.size(); i++) {
        OverheadLabelComponent* l = oveheadLabels.getReference(i);
        mediaComponent->removeChildComponent(l);

        delete l;
    }*/

    oveheadLabels.clear();

    resized();
    repaint();
}

void MediaDisplayComponent::setNewTarget(URL filePath)
{
    targetFilePath = filePath;

    addNewTempFile();
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
    float visibleStartX = visibleRange.getStart() * getPixelsPerSecond();
    float offsetX = ((float) getTimeAtOrigin()) * getPixelsPerSecond();

    float dX = controlSpacing + getMediaXPos() + mX - (visibleStartX - offsetX);

    return dX;
}

void MediaDisplayComponent::resetTransport()
{
    transportSource.stop();
    transportSource.setSource(nullptr);
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

void MediaDisplayComponent::resetPaths()
{
    clearDroppedFile();

    targetFilePath = URL();

    tempFilePaths.clear();
    currentTempFileIdx = -1;
}

// TODO - may be able to simplify some of this logic by embedding cursor in media component
void MediaDisplayComponent::updateCursorPosition()
{
    bool displayCursor =
        isFileLoaded() && (isPlaying() || getMediaComponent()->isMouseButtonDown(true));

    float cursorPositionX = mediaXToDisplayX(timeToMediaX(getPlaybackPosition()));

    Rectangle<int> mediaBounds = mediaComponent.getLocalBounds();

    float cursorBoundsStartX = mediaBounds.getX() + getMediaXPos();
    float cursorBoundsWidth = visibleRange.getLength() * getPixelsPerSecond();

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
    cursorPositionX += mediaComponent.getBounds().getX();

    currentPositionMarker.setRectangle(Rectangle<float>(
        cursorPositionX, mediaBounds.getY(), cursorWidth, mediaBounds.getHeight()));
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
    // DBG("Mouse wheel moved: deltaX=" << wheel.deltaX << ", deltaY=" << wheel.deltaY << ", scrollPos:" << evt.position.getX());

    if (getTotalLengthInSecs() > 0.0)
    {
        bool isCmdPressed = evt.mods.isCommandDown(); // Command key
        bool isShiftPressed = evt.mods.isShiftDown(); // Shift key
        bool isCtrlPressed = evt.mods.isCtrlDown(); // Control key

#if JUCE_MAC
        bool zoomMod = isCmdPressed;
#else
        bool zoomMod = isCtrlPressed;
#endif

        auto totalLength = visibleRange.getLength();
        auto visibleStart = visibleRange.getStart();
        auto scrollTime = mediaXToTime(evt.position.getX());
        DBG("Visible range: (" << visibleStart << ", " << visibleStart + totalLength
                               << ") Scrolled at time: " << scrollTime);

        if (std::abs(wheel.deltaX) > 2 * std::abs(wheel.deltaY))
        {
            // Horizontal scroll when using 2-finger swipe in macbook trackpad
            horizontalMove(wheel.deltaX);
        }
        else if (std::abs(wheel.deltaY) > 2 * std::abs(wheel.deltaX))
        {
            // if (wheel.deltaY != 0) {
            horizontalZoom(wheel.deltaY, scrollTime);

            // }
        }
        else
        {
            // Do nothing
        }

        repaint();
    }
}
