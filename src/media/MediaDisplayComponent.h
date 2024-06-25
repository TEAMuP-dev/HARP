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

    static StringArray getSupportedExtensions();

    void loadMediaFile(const URL& filePath);

    URL getTargetFilePath()
    {
        return targetFilePath;
    }

    bool isFileLoaded()
    {
        return !targetFilePath.isEmpty();
    }

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

    URL getTempFilePath()
    {
        return tempFilePath;
    }

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

    void startPlaying();

    void stopPlaying();

    bool isPlaying();

    // TODO - cursor functionality

    // TODO - zoom functionality

    void filesDropped (const StringArray& files, int /*x*/, int /*y*/) override
    {
        URL firstFilePath = URL(File(files[0]));

        lastActionType = FileDropped;
        sendChangeMessage();

        loadMediaFile(firstFilePath);
        generateTempFile();
    }

    enum ActionType {
        FileDropped,
        TransportMoved,
        TransportStarted
    };

    ActionType getLastActionType() const noexcept { return lastActionType; }

protected:

    void setNewTarget(URL filePath)
    {
        targetFilePath = filePath;
        //tempFilePath = URL();
        generateTempFile();
    }

private:

    URL targetFilePath;
    URL tempFilePath;

    ActionType lastActionType;
};
