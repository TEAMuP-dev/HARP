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
        setupDisplay();
    }

    void setupDisplay();

    void drawMainArea(Graphics& g);

    void paint(Graphics& g) override
    {
        g.fillAll (Colours::darkgrey);
        g.setColour (Colours::lightblue);

        if (isFileLoaded())
        {
            auto area = getLocalBounds();

            //area.removeFromBottom(scrollbar.getHeight() + 4);
            drawMainArea(g);
        }
        else
        {
            g.setFont(14.0f);
            g.drawFittedText ("No audio file selected", getLocalBounds(), Justification::centred, 2);
        }
    }

    void changeListenerCallback(ChangeBroadcaster*) override
    {
        // this method is called by the thumbnail when it has changed, so we should repaint it.
        repaint();
    }

    static StringArray getSupportedExtensions();

    void loadMediaFile(const URL& filePath);

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

    bool isInterestedInFileDrag (const StringArray& /*files*/) override { return true; }

    void filesDropped (const StringArray& files, int /*x*/, int /*y*/) override
    {
        URL firstFilePath = URL(File(files[0]));

        lastActionType = FileDropped;
        sendChangeMessage();

        loadMediaFile(firstFilePath);
        generateTempFile();
    }

    void setPlaybackPosition(float x);

    float getPlaybackPosition();

    void mouseDown(const MouseEvent& e) override { mouseDrag(e); }

    void mouseDrag (const MouseEvent& e) override
    {
        if (canMoveTransport()) {
            setPlaybackPosition((float) e.x);
            lastActionType = TransportMoved;
        }
    }

    void mouseUp (const MouseEvent&) override
    {
        if (lastActionType == TransportMoved) {
            // transportSource.start();
            lastActionType = TransportStarted;
            sendChangeMessage();
        }
        
    }

    void startPlaying();

    void stopPlaying();

    bool isPlaying();

    // TODO - zoom functionality

    String getMediaHandlerInstructions() { return mediaHandlerInstructions; }

protected:

    void setNewTarget(URL filePath)
    {
        targetFilePath = filePath;
        //tempFilePath = URL();
        generateTempFile();
    }

    String mediaHandlerInstructions;

    double xToTime (const float x) const
    {
        return (x / (float) getWidth()) * (visibleRange.getLength()) + visibleRange.getStart();
    }

    float timeToX (const double time) const
    {
        if (visibleRange.getLength() <= 0) {
            return 0;
        }

        return (float) getWidth() * (float) ((time - visibleRange.getStart()) / visibleRange.getLength());
    }

private:

    bool canMoveTransport() { return !isPlaying(); }

    void updateCursorPosition()
    {
        currentPositionMarker.setVisible(isPlaying() || isMouseButtonDown());
        currentPositionMarker.setRectangle(Rectangle<float>(timeToX(getPlaybackPosition()) - 0.75f, 0, 1.5f,
        (float) (getHeight() - scrollbar.getHeight())));
    }

    void timerCallback() override
    {
        if (canMoveTransport())
            updateCursorPosition();
        else
            setRange(visibleRange.movedToStartAt(getPlaybackPosition() - (visibleRange.getLength() / 2.0)));
    }

    URL targetFilePath;
    URL tempFilePath;

    ActionType lastActionType;

    DrawableRectangle currentPositionMarker;
};
