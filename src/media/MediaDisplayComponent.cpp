#include "MediaDisplayComponent.h"

MediaDisplayComponent::MediaDisplayComponent()
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

    addAndMakeVisible(overheadPanel);
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
    repositionOverheadPanel();
    repositionContent();
    repositionScrollBar();
    repositionLabels();
}

void MediaDisplayComponent::repositionOverheadPanel()
{
    if (getNumOverheadLabels())
    {
        overheadPanel.setBounds(getLocalBounds()
                                    .removeFromTop(labelHeight + 2 * controlSpacing + 2)
                                    .reduced(controlSpacing));
    }
    else
    {
        overheadPanel.setBounds(getLocalBounds().removeFromTop(0));
    }
}

Rectangle<int> MediaDisplayComponent::getContentBounds()
{
    Rectangle<int> contentBounds = getLocalBounds()
        .removeFromTop(getHeight() - (scrollBarSize + 2 * controlSpacing));

    if (getNumOverheadLabels())
    {
        contentBounds = contentBounds.withTrimmedTop(labelHeight + 2 * controlSpacing + 2);
    }

    return contentBounds.reduced(controlSpacing);
}

void MediaDisplayComponent::repositionScrollBar()
{
    horizontalScrollBar.setBounds(getLocalBounds()
                                      .removeFromBottom(scrollBarSize + 2 * controlSpacing)
                                      .reduced(controlSpacing));
}

void MediaDisplayComponent::repositionLabels()
{
    if (! visibleRange.getLength())
    {
        return;
    }

    float mediaWidth = getMediaWidth();
    float mediaHeight = getMediaHeight();

    float pixelsPerSecond = mediaWidth / visibleRange.getLength();

    float minLabelWidth = 0.1 * mediaWidth;
    float maxLabelWidth = 0.10 * pixelsPerSecond;

    float contentWidth = getContentBounds().getWidth();
    float minVisibilityWidth = contentWidth / 200.0f;
    float maxVisibilityWidth = contentWidth / 3.0f;

    minLabelWidth = jmin(minLabelWidth, maxVisibilityWidth);
    maxLabelWidth = jmax(maxLabelWidth, minVisibilityWidth);

    auto positionLabels = [this, minLabelWidth, maxLabelWidth, mediaHeight](auto labels) {
        for (auto l : labels)
        {
            float labelWidth = jmax(minLabelWidth, jmin(maxLabelWidth, l->getTextWidth() + 2 * textSpacing));

            float labelStartTime = l->getTime();
            float labelStopTime = labelStartTime + l->getDuration();

            float xPos = correctToBounds(timeToMediaX(labelStartTime + l->getDuration() / 2) - labelWidth / 2.0f, labelWidth);
            float yPos = 1.0f;

            if (auto lo = dynamic_cast<LabelOverlayComponent*>(l))
            {
                yPos = lo->getRelativeY() * mediaHeight;
                yPos -= labelHeight / 2.0f;
                yPos = jmin(mediaHeight - labelHeight, jmax(0.0f, yPos));
            }

            l->setBounds(xPos, yPos, labelWidth, labelHeight);
            l->toFront(true);

            float leftLabelMarkerPos = correctToBounds(timeToMediaX(labelStartTime), cursorWidth / 2);
            l->setLeftMarkerBounds(Rectangle<float>(
                leftLabelMarkerPos, 0, cursorWidth, mediaHeight).toNearestInt());

            float rightLabelMarkerPos = correctToBounds(timeToMediaX(labelStopTime), cursorWidth / 2);
            l->setRightMarkerBounds(Rectangle<float>(
                rightLabelMarkerPos, 0, cursorWidth, mediaHeight).toNearestInt());

            float durationWidth = jmax(0.0f, rightLabelMarkerPos - leftLabelMarkerPos - cursorWidth / 2);
            l->setDurationFillBounds(Rectangle<float>(
                leftLabelMarkerPos + cursorWidth / 2, 0, durationWidth, mediaHeight).toNearestInt());

            if (l->getIndex() == currentTempFileIdx) {
                l->setVisible(true);
            } else {
                l->setVisible(false);
            }
        }
    };

    positionLabels(overheadLabels);
    positionLabels(labelOverlays);
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
    sendChangeMessage();

    currentHorizontalZoomFactor = 1.0;
    horizontalScrollBar.setRangeLimits({ 0.0, 1.0 });
    horizontalScrollBar.setVisible(false);
}

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

    File originalFile = targetFilePath.getLocalFile();

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
    if (getTargetFilePath() == URL(File(files[0]))) {
        DBG("Won't self drag");
        return;
    }

    droppedFilePath = URL(File(files[0]));
    sendChangeMessage();
}

void MediaDisplayComponent::mouseDrag(const MouseEvent& e)
{
    if (e.eventComponent == getMediaComponent() && ! isPlaying() && isMouseOver(true))
    {
        float x_ = (float) e.x;

        double visibleStart = visibleRange.getStart();
        double visibleStop = visibleStart + visibleRange.getLength();

        x_ = jmax(timeToMediaX(visibleStart), x_);
        x_ = jmin(timeToMediaX(visibleStop), x_);

        setPlaybackPosition(mediaXToTime(x_));
    }

    if (!isMouseOver(true))
    {
        performExternalDragDropOfFiles(StringArray(getTargetFilePath().getLocalFile().getFullPathName()), true);

        if (! isPlaying())
        {
            setPlaybackPosition(0.0);
        }
    }

    updateCursorPosition();
}

void MediaDisplayComponent::mouseUp(const MouseEvent& e)
{
    mouseDrag(e); // make sure playback position has been updated

    for (OverheadLabelComponent* label : overheadLabels)
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

    if (e.eventComponent == getMediaComponent() && isMouseOver(true)) //Only start playback if we're still in this area
    {
        start();
        sendChangeMessage();
    }
}

void MediaDisplayComponent::start()
{
    startPlaying();

    startTimerHz(40);
}

void MediaDisplayComponent::stop()
{
    stopPlaying();

    stopTimer();

    currentPositionMarker.setVisible(false);
    setPlaybackPosition(0.0);
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

void MediaDisplayComponent::addLabels(LabelList& labels)
{
    for (const auto& l : labels)
    {
        std::unique_ptr<OutputLabelComponent> lc = std::make_unique<OutputLabelComponent>((double)l->t, l->label);;

        if ((l->description).has_value()) {
            lc->setDescription((l->description).value());
        }

        if ((l->duration).has_value()) {
            lc->setDuration((double) (l->duration).value());
        }

        if ((l->color).has_value()) {
            lc->setColor(Colour((l->color).value()));
        }

        if ((l->link).has_value()) {
            lc->setLink((l->link).value());
        }

        float y;

        bool isOverlay = false;

        if (auto audioLabel = dynamic_cast<AudioLabel*>(l.get())) {
            if ((audioLabel->amplitude).has_value()) {
                isOverlay = true;

                float amp = (audioLabel->amplitude).value();

                y = LabelOverlayComponent::amplitudeToRelativeY(amp);
            }
        }

        if (auto midiLabel = dynamic_cast<MidiLabel*>(l.get())) {
            if ((midiLabel->pitch).has_value()) {
                isOverlay = true;

                float p = (midiLabel->pitch).value();

                y = LabelOverlayComponent::pitchToRelativeY(p);
            }
        }

        if (isOverlay) {
            auto lo = static_cast<LabelOverlayComponent*>(lc.get());
            lo->setRelativeY(y);

            addLabelOverlay(*lo);
        } else {
            auto ol = static_cast<OverheadLabelComponent*>(lc.get());
            addOverheadLabel(*ol);
        }
    }
}

void MediaDisplayComponent::addLabelOverlay(LabelOverlayComponent l)
{
    LabelOverlayComponent* label = new LabelOverlayComponent(l);
    label->setFont(Font(jmax(minFontSize, labelHeight - 2 * textSpacing)));
    label->setIndex(currentTempFileIdx);
    labelOverlays.add(label);

    Component* mediaComponent = getMediaComponent();
    mediaComponent->addAndMakeVisible(label);
    label->addMarkersTo(mediaComponent);
}

void MediaDisplayComponent::addOverheadLabel(OverheadLabelComponent l)
{
    OverheadLabelComponent* label = new OverheadLabelComponent(l);
    label->setFont(Font(jmax(minFontSize, labelHeight - 2 * textSpacing)));
    label->setIndex(currentTempFileIdx);
    overheadLabels.add(label);

    overheadPanel.addAndMakeVisible(label);

    Component* mediaComponent = getMediaComponent();
    label->addMarkersTo(mediaComponent);
}

void MediaDisplayComponent::clearLabels(int processingIdxCutoff)
{
    for (int i = labelOverlays.size() - 1; i >= 0; --i)
    {
        LabelOverlayComponent* l = labelOverlays.getReference(i);

        if (l->getIndex() >= processingIdxCutoff) {
            removeLabelOverlay(l);
        }
    }

    if (!processingIdxCutoff) {
        labelOverlays.clear();
    }

    for (int i = overheadLabels.size() - 1; i >= 0; --i)
    {
        OverheadLabelComponent* l = overheadLabels.getReference(i);

        if (l->getIndex() >= processingIdxCutoff) {
            removeOverheadLabel(l);
        }
    }

    if (!processingIdxCutoff) {
        overheadLabels.clear();
    }

    resized();
    repaint();
}

void MediaDisplayComponent::removeLabelOverlay(LabelOverlayComponent* l)
{
    Component* mediaComponent = getMediaComponent();

    l->removeMarkersFrom(mediaComponent);
    mediaComponent->removeChildComponent(l);

    labelOverlays.removeFirstMatchingValue(l);

    delete l;
}

void MediaDisplayComponent::removeOverheadLabel(OverheadLabelComponent* l)
{
    Component* mediaComponent = getMediaComponent();

    l->removeMarkersFrom(mediaComponent);
    overheadPanel.removeChildComponent(l);

    overheadLabels.removeFirstMatchingValue(l);

    delete l;
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

void MediaDisplayComponent::resetPaths()
{
    clearDroppedFile();

    targetFilePath = URL();

    tempFilePaths.clear();
    currentTempFileIdx = -1;
}

int MediaDisplayComponent::correctToBounds(float x, float width) {

    x = jmax(timeToMediaX(0.0), x);
    x = jmin(timeToMediaX(getTotalLengthInSecs()) - width, x);

    return x;
}

// TODO - may be able to simplify some of this logic by embedding cursor in media component
void MediaDisplayComponent::updateCursorPosition()
{
    bool displayCursor =
        isFileLoaded() && (isPlaying() || (getMediaComponent()->isMouseButtonDown(true) && isMouseOver(true)));

    float cursorPositionX = mediaXToDisplayX(timeToMediaX(getPlaybackPosition()));

    Rectangle<int> mediaBounds = getContentBounds();

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
