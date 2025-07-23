/*
 * @file TrackAreaWidget.h
 * @brief The component that displays a group of tracks in the GUI.
 * @author xribene, cwitkowitz
 */

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include "media/AudioDisplayComponent.h"
#include "media/MediaDisplayComponent.h"
#include "media/MidiDisplayComponent.h"
#include "utils.h"

using namespace juce;

class TrackAreaWidget : public Component, public ChangeListener, public FileDragAndDropTarget
{
public:
    TrackAreaWidget(DisplayMode mode = DisplayMode::Input, int height = 0)
        : displayMode(mode), fixedHeight(height)
    {
    }

    void paint(Graphics& g) override
    {
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized() override
    {
        FlexBox mainBox;

        mainBox.flexDirection = FlexBox::Direction::column;

        if (getNumTracks() > 0)
        {
            for (auto& m : mediaDisplays)
            {
                FlexItem i = FlexItem(*m);

                if (fixedHeight)
                {
                    i = i.withHeight(fixedHeight);
                }
                else
                {
                    i = i.withFlex(1).withMinHeight(50);
                }

                mainBox.items.add(i.withMargin(4));
            }
        }

        mainBox.performLayout(getLocalBounds());
    }

    std::vector<std::unique_ptr<MediaDisplayComponent>>& getMediaDisplays()
    {
        return mediaDisplays;
    }

    int getNumTracks() { return mediaDisplays.size(); }

    bool isInputWidget() { return (displayMode == 0) || isHybridWidget(); }
    bool isOutputWidget() { return (displayMode == 1) || isHybridWidget(); }
    bool isHybridWidget() { return displayMode == 2; }
    bool isThumbnailWidget() { return displayMode == 3; }

    bool isInterestedInFileDrag(const StringArray& /*files*/) override
    {
        return isThumbnailWidget();
    }

    void resetUI()
    {
        // TODO - is this necessary?
        // TODO - does this need to go in the destructor?

        for (auto& m : mediaDisplays)
        {
            removeChildComponent(m.get());
            m->removeChangeListener(this);
        }

        mediaDisplays.clear();
    }

    void addTrack(ComponentInfo info)
    {
        std::shared_ptr<PyHarpComponentInfo> trackInfo = info.second;
        std::unique_ptr<MediaDisplayComponent> m;

        std::string label =
            trackInfo->label.empty() ? "Track-" + std::to_string(getNumTracks()) : trackInfo->label;

        if (auto audioTrackInfo = dynamic_cast<AudioTrackInfo*>(trackInfo.get()))
        {
            m = std::make_unique<AudioDisplayComponent>(
                label, audioTrackInfo->required, false, displayMode);
        }
        else if (auto midiTrackInfo = dynamic_cast<MidiTrackInfo*>(trackInfo.get()))
        {
            m = std::make_unique<MidiDisplayComponent>(
                label, midiTrackInfo->required, false, displayMode);
        }

        if (m)
        {
            m->setDisplayID(trackInfo->id);
            m->addChangeListener(this);
            addAndMakeVisible(m.get());

            if (m->isThumbnailTrack())
            {
                m->selectTrack();
            }

            mediaDisplays.push_back(std::move(m));
        }

        resized();
    }

    void filesDropped(const StringArray& files, int /*x*/, int /*y*/)
    {
        bool duplicateDetected = false; // Global duplicate flag

        for (String f : files)
        {
            File droppedFile = File(f);
            URL droppedFilePath = URL(droppedFile);

            bool isTrackDuplicate = false; // Track-level duplicate flag

            for (auto& m : mediaDisplays)
            {
                if (m->isDuplicateFile(droppedFilePath))
                {
                    if (! duplicateDetected)
                    {
                        m->selectTrack();
                    }

                    duplicateDetected = true; // Select first duplicate (only)
                    isTrackDuplicate = true; // Don't reload track

                    DBG("TrackAreaWidget::filesDropped: Selecting existing track containing "
                        << f << " instead of creating new track.");
                }
            }

            if (! isTrackDuplicate)
            {
                String ext = droppedFile.getFileExtension();

                bool validExt = true;

                ComponentInfo componentInfo;

                if (AudioDisplayComponent::getSupportedExtensions().contains(ext))
                {
                    auto audioTrackInfo = std::make_shared<AudioTrackInfo>();
                    audioTrackInfo->id = Uuid();
                    audioTrackInfo->required = false;
                    componentInfo = ComponentInfo(audioTrackInfo->id, audioTrackInfo);
                }
                else if (MidiDisplayComponent::getSupportedExtensions().contains(ext))
                {
                    auto midiTrackInfo = std::make_shared<MidiTrackInfo>();
                    midiTrackInfo->id = Uuid();
                    midiTrackInfo->required = false;
                    componentInfo = ComponentInfo(midiTrackInfo->id, midiTrackInfo);
                }
                else
                {
                    DBG("TrackAreaWidget::filesDropped: Tried to add file "
                        << f << " with unsupported type.");

                    validExt = false;
                }

                if (validExt)
                {
                    addTrack(componentInfo);
                    mediaDisplays.back()->initializeDisplay(droppedFilePath);
                    mediaDisplays.back()->setTrackName(droppedFilePath.getFileName());
                }
            }
        }
    }

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override
    {
        for (auto& m : mediaDisplays)
        {
            if (source != m.get())
            {
                m->deselectTrack();
            }
        }
    }

    const int fixedHeight = 0;
    const DisplayMode displayMode;

    std::vector<std::unique_ptr<MediaDisplayComponent>> mediaDisplays;
};
