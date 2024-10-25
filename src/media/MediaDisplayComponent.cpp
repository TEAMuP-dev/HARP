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

    if (!isFileLoaded()) {
        g.setFont(14.0f);
        g.drawFittedText("No media file selected...", getLocalBounds(), Justification::centred, 2);
    }
}

void MediaDisplayComponent::resized()
{
    repositionContent();
    repositionScrollBar();
    repositionLabels();
}

Rectangle<int> MediaDisplayComponent::getContentBounds()
{
    return getLocalBounds().removeFromTop(getHeight() - (scrollBarSize + 2 * controlSpacing)).reduced(controlSpacing);
}

void MediaDisplayComponent::repositionScrollBar()
{
    horizontalScrollBar.setBounds(getLocalBounds().removeFromBottom(scrollBarSize + 2 * controlSpacing).reduced(controlSpacing));
}

void MediaDisplayComponent::repositionOverheadLabels()
{
    // for (auto l : oveheadLabels) {}
}

void MediaDisplayComponent::repositionLabelOverlays()
{
    if (!visibleRange.getLength()) {
        return;
    }

    float mediaHeight = getMediaHeight();
    float mediaWidth = getMediaWidth();

    float pixelsPerSecond = mediaWidth / visibleRange.getLength();

    float minLabelWidth = 0.0015 * pixelsPerSecond;
    float maxLabelWidth = 0.10 * pixelsPerSecond;

    float contentWidth = getContentBounds().getWidth();
    float minVisibilityWidth = contentWidth / 200.0f;
    float maxVisibilityWidth = contentWidth / 3.0f;

    minLabelWidth = jmin(minLabelWidth, maxVisibilityWidth);
    maxLabelWidth = jmax(maxLabelWidth, minVisibilityWidth);

    for (auto l : labelOverlays) {
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
    sendChangeMessage();

    currentHorizontalZoomFactor = 1.0;
    horizontalScrollBar.setRangeLimits({0.0, 1.0});
    horizontalScrollBar.setVisible(false);
}

void MediaDisplayComponent::setupDisplay(const URL& filePath)
{
    resetMedia();

    setNewTarget(filePath);
    updateDisplay(filePath);

    horizontalScrollBar.setVisible(true);
    updateVisibleRange({0.0, getTotalLengthInSecs()});
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

    if (!numTempFiles) {
        targetFile = originalFile;
    } else {
        targetFile = getTempFilePath().getLocalFile();
    }

    String docsDirectory = File::getSpecialLocation(File::SpecialLocationType::userDocumentsDirectory).getFullPathName();

    String targetFileName = originalFile.getFileNameWithoutExtension();
    String targetFileExtension = originalFile.getFileExtension();

    URL tempFilePath = URL(File(docsDirectory + "/HARP/" + targetFileName + "_" + String(numTempFiles) + targetFileExtension));

    File tempFile = tempFilePath.getLocalFile();

    tempFile.getParentDirectory().createDirectory();

    if (!targetFile.copyFileTo(tempFile)) {
        DBG("MediaDisplayComponent::generateTempFile: Failed to copy file " << targetFile.getFullPathName() << " to " << tempFile.getFullPathName() << ".");

        AlertWindow("Error", "Failed to create temporary file for processing.", AlertWindow::WarningIcon);
    } else {
        DBG("MediaDisplayComponent::generateTempFile: Copied file " << targetFile.getFullPathName() << " to " << tempFile.getFullPathName() << ".");
    }

    tempFilePaths.add(tempFilePath);
    currentTempFileIdx++;
}

bool MediaDisplayComponent::iteratePreviousTempFile()
{
    if (currentTempFileIdx > 0) {
        currentTempFileIdx--;

        updateDisplay(getTempFilePath());

        return true;
    } else {
        return false;
    }
}

bool MediaDisplayComponent::iterateNextTempFile()
{
    if (currentTempFileIdx + 1 < tempFilePaths.size()) {
        currentTempFileIdx++;

        updateDisplay(getTempFilePath());

        return true;
    } else {
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

    File backupFile = File(parentDirectory + "/" + targetFileName + "_BACKUP" + targetFileExtension);

    if (targetFile.copyFileTo(backupFile)) {
        DBG("MediaDisplayComponent::overwriteTarget: Created backup of file" << targetFile.getFullPathName() << " at "  << backupFile.getFullPathName() << ".");
    } else {
        DBG("MediaDisplayComponent::overwriteTarget: Failed to create backup of file" << targetFile.getFullPathName() << " at "  << backupFile.getFullPathName() << ".");
    }

    if (tempFile.copyFileTo(targetFile)) {
        DBG("MediaDisplayComponent::overwriteTarget: Overwriting file " << targetFile.getFullPathName() << " with " << tempFile.getFullPathName() << ".");
    } else {
        DBG("MediaDisplayComponent::overwriteTarget: Failed to overwrite file " << targetFile.getFullPathName() << " with " << tempFile.getFullPathName() << ".");
    }
}

void MediaDisplayComponent::filesDropped(const StringArray& files, int /*x*/, int /*y*/)
{
    // TODO - warning or handling for additional files

    droppedFilePath = URL(File(files[0]));
    sendChangeMessage();
}

void MediaDisplayComponent::mouseDrag(const MouseEvent& e)
{
    if (e.eventComponent == getMediaComponent() && !isPlaying()) {
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

    if (e.eventComponent == getMediaComponent()) {
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

    for (OverheadLabelComponent* label : oveheadLabels)
    {
        if (label->isMouseOver()) {
            toolTipText = label->getDescription();
        }
    }

    for (LabelOverlayComponent* label : labelOverlays)
    {
        if (label->isMouseOver()) {
            toolTipText = label->getDescription();
        }
    }

    return toolTipText;
}

void MediaDisplayComponent::addLabels(LabelList& labels)
{
    clearLabels();

    for (const auto& l : labels) {
        String lbl = l->label;
        String dsc = l->description;

        if (dsc.isEmpty()) {
            dsc = lbl;
        }

        float dur = 0.0f;

        if ((l->duration).has_value()) {
            dur = (l->duration).value();
        }

        Colour color = Colours::purple.withAlpha(0.8f);

        if ((l->color).has_value()) {
            color = Colour((l->color).value());
        }

        if (!dynamic_cast<AudioLabel*>(l.get()) &&
            !dynamic_cast<SpectrogramLabel*>(l.get()) &&
            !dynamic_cast<MidiLabel*>(l.get())) {
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

    for (int i = 0; i < labelOverlays.size(); i++) {
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

    if (visibleRange.getLength() <= 0) {
        x = 0;
    } else {
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
    bool displayCursor = isFileLoaded() && (isPlaying() || getMediaComponent()->isMouseButtonDown(true));

    float cursorPositionX = mediaXToDisplayX(timeToMediaX(getPlaybackPosition()));

    Rectangle<int> mediaBounds = getContentBounds();

    float cursorBoundsStartX = mediaBounds.getX() + getMediaXPos();
    float cursorBoundsWidth = visibleRange.getLength() * getPixelsPerSecond();

    // TODO - due to very small differences, cursor may not be visible at media bounds when zoomed in
    if (cursorPositionX >= cursorBoundsStartX && cursorPositionX <= (cursorBoundsStartX + cursorBoundsWidth)) {
        currentPositionMarker.setVisible(displayCursor);
    } else {
        currentPositionMarker.setVisible(false);
    }

    cursorPositionX -= cursorWidth / 2.0f;

    currentPositionMarker.setRectangle(Rectangle<float>(cursorPositionX, mediaBounds.getY(), cursorWidth, mediaBounds.getHeight()));
}

void MediaDisplayComponent::timerCallback()
{
    if (isPlaying()) {
        updateVisibleRange(visibleRange);
    } else {
        stop();
        sendChangeMessage();
    }
}

void MediaDisplayComponent::scrollBarMoved(ScrollBar* scrollBarThatHasMoved, double scrollBarRangeStart)
{
    if (scrollBarThatHasMoved == &horizontalScrollBar) {
        updateVisibleRange(visibleRange.movedToStartAt(scrollBarRangeStart));
    }
}

void MediaDisplayComponent::mouseWheelMove(const MouseEvent&, const MouseWheelDetails& wheel)
{
    if (getTotalLengthInSecs() > 0.0)
    {
        auto totalLength = visibleRange.getLength();
        auto visibleStart = visibleRange.getStart();

        if (std::abs(wheel.deltaX) > 2 * std::abs(wheel.deltaY)) {
            auto newStart = visibleStart - wheel.deltaX * totalLength / 10.0;
            newStart = jlimit(0.0, jmax(0.0, getTotalLengthInSecs() - totalLength), newStart);

            if (!isPlaying()) {
                updateVisibleRange({ newStart, newStart + totalLength });
            }
        } else if (std::abs(wheel.deltaY) > 2 * std::abs(wheel.deltaX)) {
            if (wheel.deltaY != 0) {
                currentHorizontalZoomFactor = jlimit(1.0, 1.99, currentHorizontalZoomFactor + wheel.deltaY);

                auto newScale = jmax(0.01, getTotalLengthInSecs() * (2 - currentHorizontalZoomFactor));
                auto timeAtCenter = visibleRange.getStart() + visibleRange.getLength() / 2.0;

                updateVisibleRange({ timeAtCenter - newScale * 0.5, timeAtCenter + newScale * 0.5 });
            }
        } else {
            // Do nothing
        }

        repaint();
    }
}
