#pragma once

#include "juce_core/juce_core.h"

using namespace juce;


class MediaDisplayComponent : public Component,
                              public ChangeListener,
                              public ChangeBroadcaster,
                              public FileDragAndDropTarget,
                              private Timer,
                              private ScrollBar::Listener
{
public:

    MediaDisplayComponent()
    {
        resetPaths();

        addChildComponent(scrollbar);
        scrollbar.setAutoHide(false);
        scrollbar.addListener(this);

        currentPositionMarker.setFill(Colours::white.withAlpha(0.85f));
        addAndMakeVisible(currentPositionMarker);
    }

    ~MediaDisplayComponent()
    {
        scrollbar.removeListener(this);
    }

    virtual void drawMainArea(Graphics& g, Rectangle<int>& a) = 0;

    void paint(Graphics& g) override
    {
        g.fillAll(Colours::darkgrey);
        g.setColour(Colours::lightblue);

        if (isFileLoaded()) {
            Rectangle<int> a = getLocalBounds();

            drawMainArea(g, a);
        } else {
            g.setFont(14.0f);
            g.drawFittedText ("No media file selected...", getLocalBounds(), Justification::centred, 2);
        }
    }

    void resized() override
    {
        scrollbar.setBounds(getLocalBounds().removeFromBottom(14).reduced(2));
    }

    void changeListenerCallback(ChangeBroadcaster*) override
    {
        // Repaint whenever media has changed
        repaint();
    }

    virtual void loadMediaFile(const URL& filePath) = 0;

    void resetMedia()
    {
        resetPaths();
        resetDisplay();
        sendChangeMessage();

        currentHorizontalZoomFactor = 1.0;
        scrollbar.setRangeLimits({0.0, 1.0});
        scrollbar.setVisible(false);
    }

    void setupDisplay(const URL& filePath)
    {
        resetMedia();

        setNewTarget(filePath);

        loadMediaFile(filePath);

        postLoadActions(filePath);

        Range<double> range (0.0, getTotalLengthInSecs());

        scrollbar.setRangeLimits(range);
        scrollbar.setVisible(true);
        updateVisibleRange(range);
    }

    URL getTargetFilePath() { return targetFilePath; }

    bool isFileLoaded() { return !targetFilePath.isEmpty(); }

    void generateTempFile()
    {
        // TODO - should support temp files for intermediate steps with .n extensions

        String docsDirectory = File::getSpecialLocation(File::SpecialLocationType::userDocumentsDirectory).getFullPathName();

        File targetFile = targetFilePath.getLocalFile();

        String targetFileName = targetFile.getFileNameWithoutExtension();
        String targetFileExtension = targetFile.getFileExtension();

        tempFilePath = URL(File(docsDirectory + "/HARP/" + targetFileName + "_harp" + targetFileExtension));

        File tempFile = tempFilePath.getLocalFile();

        tempFile.getParentDirectory().createDirectory();

        if (!targetFile.copyFileTo(tempFile)) {
            DBG("MediaDisplayComponent::generateTempFile: Failed to copy file " << targetFile.getFullPathName() << " to " << tempFile.getFullPathName() << ".");

            AlertWindow("Error", "Failed to create temporary file for processing.", AlertWindow::WarningIcon);
        } else {
            DBG("MediaDisplayComponent::generateTempFile: Copied file " << targetFile.getFullPathName() << " to " << tempFile.getFullPathName() << ".");
        }
    }

    URL getTempFilePath() { return tempFilePath; }

    void overwriteTarget()
    {
        // TODO - should support backup files for intermediate steps with .n extensions

        File targetFile = targetFilePath.getLocalFile();
        File tempFile = tempFilePath.getLocalFile();

        String parentDirectory = targetFile.getParentDirectory().getFullPathName();
        String targetFileName = targetFile.getFileNameWithoutExtension();
        String targetFileExtension = targetFile.getFileExtension();

        File backupFile = File(parentDirectory + "/" + targetFileName + "_BACKUP" + targetFileExtension);

        if (targetFile.copyFileTo(backupFile)) {
            DBG("MediaDisplayComponent::overwriteTarget: Created backup of file" << targetFile.getFullPathName() << " at "  << backupFile.getFullPathName() << ".");
        } else {
            DBG("MediaDisplayComponent::overwriteTarget: Failed to create backup of file" << targetFile.getFullPathName() << " at "  << backupFile.getFullPathName() << ".");
        }

        if (tempFile.moveFileTo(targetFile)) {
            DBG("MediaDisplayComponent::overwriteTarget: Overwriting file " << targetFile.getFullPathName() << " with " << tempFile.getFullPathName() << ".");
        } else {
            DBG("MediaDisplayComponent::overwriteTarget: Failed to overwrite file " << targetFile.getFullPathName() << " with " << tempFile.getFullPathName() << ".");
        }

        generateTempFile();
    }

    bool isInterestedInFileDrag(const StringArray& /*files*/) override { return true; }

    void filesDropped (const StringArray& files, int /*x*/, int /*y*/) override
    {
        URL firstFilePath = URL(File(files[0]));

        setupDisplay(firstFilePath);
        sendChangeMessage();
    }

    virtual void setPlaybackPosition(double t) = 0;

    virtual double getPlaybackPosition() = 0;

    void mouseDown(const MouseEvent& e) override { mouseDrag(e); }

    void mouseDrag(const MouseEvent& e) override
    {
        if (!isPlaying()) {
            setPlaybackPosition(xToTime((float) e.x));
            updateCursorPosition();
        }
    }

    void mouseUp(const MouseEvent&) override
    {
        start();
        sendChangeMessage();
    }

    virtual void startPlaying() = 0;

    void start()
    {
        startPlaying();

        startTimerHz(40);
    }

    virtual void stopPlaying() = 0;

    void stop()
    {
        stopPlaying();

        stopTimer();

        currentPositionMarker.setVisible(false);
        setPlaybackPosition(0.0);
    }

    virtual bool isPlaying() = 0;

    virtual double getTotalLengthInSecs() = 0;

    void updateVisibleRange(Range<double> newRange)
    {
        visibleRange = newRange;

        scrollbar.setCurrentRange(visibleRange);
        updateCursorPosition();
        repaint();
    }

    String getMediaHandlerInstructions() { return mediaHandlerInstructions; }

protected:

    void setNewTarget(URL filePath)
    {
        targetFilePath = filePath;
        generateTempFile();
    }

    double xToTime(const float x) const
    {
        auto totalWidth = getWidth();
        auto totalLength = visibleRange.getLength();
        auto visibleStart = visibleRange.getStart();

        double t = (x / totalWidth) * totalLength + visibleStart;

        return t;
    }

    float timeToX(const double t) const
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

    Range<double> visibleRange;

    String mediaHandlerInstructions;

private:

    void resetPaths()
    {
        targetFilePath = URL();
        tempFilePath = URL();
    }

    virtual void resetDisplay() = 0;

    virtual void postLoadActions(const URL& filePath) = 0;

    void updateCursorPosition()
    {
        bool displayCursor = isPlaying() || isMouseButtonDown();

        currentPositionMarker.setVisible(displayCursor);

        float cursorHeight = (float) (getHeight() - scrollbar.getHeight());
        float cursorPosition = timeToX(getPlaybackPosition()) - (cursorWidth / 2.0f);

        currentPositionMarker.setRectangle(Rectangle<float>(cursorPosition, 0, cursorWidth, cursorHeight));
    }

    void timerCallback() override
    {
        if (isPlaying()) {
            updateVisibleRange(visibleRange.movedToStartAt(getPlaybackPosition() - (visibleRange.getLength() / 2.0f)));
        } else {
            stop();
            sendChangeMessage();
        }
    }

    void scrollBarMoved(ScrollBar* scrollBarThatHasMoved, double scrollBarRangeStart) override
    {
        if (scrollBarThatHasMoved == &scrollbar) {
            if (!isPlaying()) {
                updateVisibleRange(visibleRange.movedToStartAt(scrollBarRangeStart));
            }
        }
    }

    void mouseWheelMove(const MouseEvent&, const MouseWheelDetails& wheel) override
    {
        // DBG("Mouse wheel moved: deltaX=" << wheel.deltaX << ", deltaY=" << wheel.deltaY);

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
                    auto timeAtCenter = xToTime((float) getWidth() / 2.0f);

                    updateVisibleRange({ timeAtCenter - newScale * 0.5, timeAtCenter + newScale * 0.5 });
                }
            } else {
                // Do nothing
            }

            repaint();
        }
    }

    URL targetFilePath;
    URL tempFilePath;

    const float cursorWidth = 1.5f;
    DrawableRectangle currentPositionMarker;

    double currentHorizontalZoomFactor;
    ScrollBar scrollbar{ false };
};
