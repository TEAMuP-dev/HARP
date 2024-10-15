#include "MediaDisplayComponent.h"


MediaDisplayComponent::MediaDisplayComponent()
{
    resetPaths();

    addChildComponent(horizontalScrollBar);
    horizontalScrollBar.setAutoHide(false);
    horizontalScrollBar.addListener(this);

    currentPositionMarker.setFill(Colours::white.withAlpha(0.85f));
    addAndMakeVisible(currentPositionMarker);
}

MediaDisplayComponent::~MediaDisplayComponent()
{
    horizontalScrollBar.removeListener(this);

    clearLabels();
}

void MediaDisplayComponent::paint(Graphics& g)
{
    g.fillAll(Colours::darkgrey);
    g.setColour(Colours::lightblue);

    if (isFileLoaded()) {
        Rectangle<int> a = getLocalBounds().removeFromTop(getHeight() - (scrollBarSize + 2 * spacing)).reduced(spacing);

        paintMedia(g, a);
    } else {
        g.setFont(14.0f);
        g.drawFittedText("No media file selected...", getLocalBounds(), Justification::centred, 2);
    }
}

void MediaDisplayComponent::resized()
{
    repositionScrollBar();
    repositionLabelOverlays();
}

void MediaDisplayComponent::repositionScrollBar()
{
    horizontalScrollBar.setBounds(getLocalBounds().removeFromBottom(scrollBarSize + 2 * spacing).reduced(spacing));
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

    for (auto l : labelOverlays) {
        float textWidth = l->getFont().getStringWidthFloat(l->getText());
        float labelWidth = jmax(minLabelWidth, jmin(maxLabelWidth, textWidth + 2 * spacing));

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
        setPlaybackPosition(mediaXToTime((float) e.x));
        updateCursorPosition();
    }
}

void MediaDisplayComponent::mouseUp(const MouseEvent& e)
{
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

void MediaDisplayComponent::updateVisibleRange(Range<double> r)
{
    visibleRange = r;

    horizontalScrollBar.setCurrentRange(visibleRange);
    updateCursorPosition();
    // TODO - necessary?
    repositionLabelOverlays();
    repaint();
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

        if (!dynamic_cast<AudioLabel*>(l.get()) &&
            !dynamic_cast<SpectrogramLabel*>(l.get()) &&
            !dynamic_cast<MidiLabel*>(l.get())) {
            // TODO - OverheadLabelComponent((double) l->t, lbl, (double) dur, dsc);
        }
    }
}

void MediaDisplayComponent::addLabelOverlay(LabelOverlayComponent l)
{
    LabelOverlayComponent* label = new LabelOverlayComponent(l);
    label->setFont(Font(labelHeight - 2 * spacing));
    labelOverlays.add(label);

    getMediaComponent()->addAndMakeVisible(label);

    resized();
    repaint();
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
    float mediaWidth = getMediaWidth();
    double totalLength = visibleRange.getLength();
    double visibleStart = visibleRange.getStart();

    float x_ = jmin(mediaWidth, jmax(0.0f, x));

    double t = (x_ / mediaWidth) * totalLength + visibleStart;

    return t;
}

float MediaDisplayComponent::timeToMediaX(const double t)
{
    double totalLength = visibleRange.getLength();

    double t_ = jmin(getTotalLengthInSecs(), jmax(0.0, t));

    float x;
    
    if (totalLength <= 0) {
        x = 0;
    } else {
        double visibleStart = visibleRange.getStart();
        double visibleOffset = t_ - visibleStart;

        x = getMediaWidth() * visibleOffset / totalLength;
    }

    return x;
}

void MediaDisplayComponent::resetPaths()
{
    clearDroppedFile();

    targetFilePath = URL();

    tempFilePaths.clear();
    currentTempFileIdx = -1;
}

void MediaDisplayComponent::updateCursorPosition()
{
    bool displayCursor = isPlaying() || isMouseButtonDown();

    currentPositionMarker.setVisible(displayCursor);

    float cursorHeight = (float) (getHeight() - scrollBarSize + 2 * spacing);
    // TODO - for now, I believe this will scroll across the keys for the pianoroll display
    //        the cursor should probably be embedded in the media component
    float cursorPosition = timeToMediaX(getPlaybackPosition()) - (cursorWidth / 2.0f);

    currentPositionMarker.setRectangle(Rectangle<float>(cursorPosition, 0, cursorWidth, cursorHeight));
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
