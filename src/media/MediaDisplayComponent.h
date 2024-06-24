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

    enum ActionType {
        FileDropped,
        TransportMoved,
        TransportStarted
    };

    MediaDisplayComponent()
    {
        setupDisplay();
    }

    void setupDisplay();

    std::string getSupportedExtensions();

    void loadMediaFile(const URL& filePath);

    void startPlaying();

    void stopPlaying();

    // TODO - cursor functionality

    // TODO - zoom functionality

    // TODO - drag/drop functionality

private:
    URL targetFilePath;
    URL tempFilePath;
};
