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
}

void MediaDisplayComponent::paint(Graphics& g)
{
    g.fillAll(Colours::darkgrey);
    g.setColour(Colours::lightblue);

    if (isFileLoaded()) {
        Rectangle<int> a = getLocalBounds().removeFromTop(getHeight() - (scrollBarSize + 2 * scrollBarSpacing)).reduced(scrollBarSpacing);

        drawMainArea(g, a);
    } else {
        g.setFont(14.0f);
        g.drawFittedText("No media file selected...", getLocalBounds(), Justification::centred, 2);
    }
}

void MediaDisplayComponent::resized()
{
    horizontalScrollBar.setBounds(getLocalBounds().removeFromBottom(scrollBarSize + 2 * scrollBarSpacing).reduced(scrollBarSpacing));

    for (auto l : labels) {
        const float xPos = timeToX(l->getTime());
        const float width = ((float) l->getDuration()) * (getWidth() / getTotalLengthInSecs());

        const float yPos = l->getRelativeY() * getHeight();

        l->setBounds(xPos, yPos, width, 10.0f);
    }
}

void MediaDisplayComponent::resetMedia()
{
    resetPaths();
    resetDisplay();
    clearLabels();
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
    if (!isPlaying()) {
        setPlaybackPosition(xToTime((float) e.x));
        updateCursorPosition();
    }
}

void MediaDisplayComponent::mouseUp(const MouseEvent&)
{
    start();
    sendChangeMessage();
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

void MediaDisplayComponent::updateVisibleRange(Range<double> newRange)
{
    visibleRange = newRange;

    horizontalScrollBar.setCurrentRange(visibleRange);
    updateCursorPosition();
    repaint();
}

void MediaDisplayComponent::addLabel(LabelOverlayComponent l)
{
    LabelOverlayComponent* label = new LabelOverlayComponent(l);
    labels.add(label);

    addAndMakeVisible(label);

    resized();
    repaint();
}

void MediaDisplayComponent::removeLabel(LabelOverlayComponent* l)
{
    // TODO
}

void MediaDisplayComponent::clearLabels()
{
    // TODO

    labels.clear();
}

void MediaDisplayComponent::setNewTarget(URL filePath)
{
    targetFilePath = filePath;

    addNewTempFile();
}

double MediaDisplayComponent::xToTime(const float x) const
{
    auto totalWidth = getWidth();
    auto totalLength = visibleRange.getLength();
    auto visibleStart = visibleRange.getStart();

    double t = (x / totalWidth) * totalLength + visibleStart;

    return t;
}

float MediaDisplayComponent::timeToX(const double t) const
{
    float x;

    auto totalLength = visibleRange.getLength();

    if (totalLength <= 0) {
        x = 0;
    } else {
        auto totalWidth = (float) getWidth();
        auto visibleStart = visibleRange.getStart();
        auto visibleOffset = (float) t - visibleStart;

        x = totalWidth * visibleOffset / totalLength;
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

    float cursorHeight = (float) (getHeight() - scrollBarSize + 2 * scrollBarSpacing);
    float cursorPosition = timeToX(getPlaybackPosition()) - (cursorWidth / 2.0f);

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
