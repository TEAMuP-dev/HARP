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
        addAndMakeVisible(scrollbar);
        scrollbar.setRangeLimits(visibleRange);
        scrollbar.setAutoHide(false);
        scrollbar.addListener(this);

        currentPositionMarker.setFill(Colours::white.withAlpha (0.85f));
        addAndMakeVisible(currentPositionMarker);

        setupMediaDisplay();
    }

    virtual void setupMediaDisplay() {}

    virtual void drawMainArea(Graphics& g, Rectangle<int>& a) = 0;

    void paint(Graphics& g) override
    {
        g.fillAll (Colours::darkgrey);
        g.setColour (Colours::lightblue);

        if (isFileLoaded()) {
            Rectangle<int> a = getLocalBounds();

            drawMainArea(g, a);
        } else {
            g.setFont(14.0f);
            g.drawFittedText ("No audio file selected", getLocalBounds(), Justification::centred, 2);
        }
    }

    void resized() override
    {
        scrollbar.setBounds(getLocalBounds().removeFromBottom(14).reduced(2));
    }

    void changeListenerCallback(ChangeBroadcaster*) override
    {
        // this method is called by the media when it has changed, so we should repaint it.
        repaint();
    }

    static StringArray getSupportedExtensions();

    virtual void loadMediaFile(const URL& filePath) = 0;

    void setupMediaFile(const URL& filePath)
    {
        setNewTarget(filePath);

        loadMediaFile(filePath);

        postLoadMediaActions(filePath);

        Range<double> range (0.0, getTotalLengthInSecs());
        scrollbar.setRangeLimits(range);
        setRange(range);

        startTimerHz (40);
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

    enum ActionType {
        FileDropped,
        TransportMoved,
        TransportStarted
    };

    ActionType getLastActionType() const noexcept { return lastActionType; }

    bool isInterestedInFileDrag(const StringArray& /*files*/) override { return true; }

    void filesDropped (const StringArray& files, int /*x*/, int /*y*/) override
    {
        URL firstFilePath = URL(File(files[0]));

        lastActionType = FileDropped;
        sendChangeMessage();

        loadMediaFile(firstFilePath);
        generateTempFile();
    }

    virtual void setPlaybackPosition(float x) = 0;

    virtual float getPlaybackPosition() = 0;

    void mouseDown(const MouseEvent& event) override { mouseDrag(event); }

    void mouseDrag(const MouseEvent& event) override
    {
        if (!isPlaying()) {
            setPlaybackPosition((float) event.x);
            lastActionType = TransportMoved;
        }
    }

    void mouseUp(const MouseEvent&) override
    {
        if (lastActionType == TransportMoved) {
            //startPlaying();
            lastActionType = TransportStarted;
            sendChangeMessage();
        }
        
    }

    virtual void startPlaying() = 0;

    virtual void stopPlaying() = 0;

    virtual bool isPlaying() = 0;

    virtual double getTotalLengthInSecs() = 0;

    void setRange(Range<double> range)
    {
        visibleRange = range;

        scrollbar.setCurrentRange(range);
        updateCursorPosition();
        repaint();
    }

    String getMediaHandlerInstructions() { return mediaHandlerInstructions; }

protected:

    void setNewTarget(URL filePath)
    {
        targetFilePath = filePath;
        //tempFilePath = URL();
        generateTempFile();
    }

    double xToTime(const float x) const
    {
        return (x / (float) getWidth()) * (visibleRange.getLength()) + visibleRange.getStart();
    }

    float timeToX(const double time) const
    {
        if (visibleRange.getLength() <= 0) {
            return 0;
        }

        return (float) getWidth() * (float) ((time - visibleRange.getStart()) / visibleRange.getLength());
    }

    Range<double> visibleRange;

    String mediaHandlerInstructions;

private:

    virtual void postLoadMediaActions(const URL& filePath) = 0;

    void updateCursorPosition()
    {
        currentPositionMarker.setVisible(isPlaying() || isMouseButtonDown());
        currentPositionMarker.setRectangle(Rectangle<float>(timeToX(getPlaybackPosition()) - 0.75f, 0, 1.5f,
                                                                    (float) (getHeight() - scrollbar.getHeight())));
    }

    void timerCallback() override
    {
        if (!isPlaying()) {
            updateCursorPosition();
        } else {
            setRange(visibleRange.movedToStartAt(getPlaybackPosition() - (visibleRange.getLength() / 2.0)));
        }
    }

    void scrollBarMoved(ScrollBar* scrollBarThatHasMoved, double newRangeStart) override
    {
        if (scrollBarThatHasMoved == &scrollbar) {
            if (!isPlaying()) {
                setRange(visibleRange.movedToStartAt(newRangeStart));
            }
        }
    }

    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel) override
    {
        // DBG("Mouse wheel moved: deltaX=" << wheel.deltaX << ", deltaY=" << wheel.deltaY);
        if (getTotalLengthInSecs() > 0.0)
        {
            if (std::abs(wheel.deltaX) > 2 * std::abs(wheel.deltaY)) {
                auto start = visibleRange.getStart() - wheel.deltaX * (visibleRange.getLength()) / 10.0;
                start = jlimit(0.0, jmax(0.0, getTotalLengthInSecs() - visibleRange.getLength()), start);

                if (!isPlaying()) {
                    setRange({ start, start + visibleRange.getLength() });
                }
            } else {
                // Do nothing
            }

            repaint();
        }
    }

    URL targetFilePath;
    URL tempFilePath;

    ActionType lastActionType;

    ScrollBar scrollbar{ false };
    DrawableRectangle currentPositionMarker;
};
