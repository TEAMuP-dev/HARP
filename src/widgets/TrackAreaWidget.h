/**
 * @file TrackAreaWidget.h
 * @brief Component that displays a group of tracks in the GUI.
 * @author xribene, cwitkowitz
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../media/AudioDisplayComponent.h"
#include "../media/MediaDisplayComponent.h"
#include "../media/MidiDisplayComponent.h"

#include "../utils/Controls.h"
#include "../utils/Interface.h"
#include "../utils/Logging.h"

using namespace juce;

class TrackAreaWidget : public Component,
                        public ChangeListener,
                        public ChangeBroadcaster,
                        public FileDragAndDropTarget
{
public:
    TrackAreaWidget(DisplayMode mode = DisplayMode::Input, int trackHeight = 0)
        : displayMode(mode), fixedTrackHeight(trackHeight)
    {
    }

    ~TrackAreaWidget() { resetState(); }

    void paint(Graphics& g) override
    {
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized() override
    {
        FlexBox mainBox;

        mainBox.flexDirection = FlexBox::Direction::column;

        int totalWidth = getWidth();
        int totalHeight = getHeight();

        if (getNumTracks() > 0)
        {
            for (auto& m : mediaDisplays)
            {
                FlexItem i = FlexItem(*m);

                if (fixedTrackHeight)
                {
                    i = i.withHeight(fixedTrackHeight);
                }
                else
                {
                    i = i.withFlex(1).withMinHeight(50);
                }

                mainBox.items.add(i.withMargin(marginSize));
            }

            if (fixedTrackHeight)
            {
                int individualTrackHeight = fixedTrackHeight + static_cast<int>(2 * marginSize);

                int totalTrackAreaHeight = getNumTracks() * individualTrackHeight;

                if (totalTrackAreaHeight > minTotalHeight)
                {
                    totalHeight = totalTrackAreaHeight;
                }
                else
                {
                    totalHeight = minTotalHeight;
                }
            }
        }
        else
        {
            totalHeight = minTotalHeight;
        }

        if (fixedTotalWidth)
        {
            totalWidth = fixedTotalWidth;
        }

        if (totalWidth != getWidth() || totalHeight != getHeight())
        {
            setSize(totalWidth, totalHeight);
        }

        mainBox.performLayout(getLocalBounds());
    }

    std::vector<std::unique_ptr<MediaDisplayComponent>>& getMediaDisplays()
    {
        return mediaDisplays;
    }

    MediaDisplayComponent* getCurrentlySelectedDisplay()
    {
        for (auto& m : mediaDisplays)
        {
            if (m->isCurrentlySelected())
            {
                return m.get();
            }
        }

        return nullptr;
    }

    std::vector<MediaDisplayComponent*> getDAWLinkedDisplays()
    {
        std::vector<MediaDisplayComponent*> linkedDisplays;

        for (auto& m : mediaDisplays)
        {
            if (m->isLinkedToDAW())
            {
                linkedDisplays.push_back(m.get());
            }
        }

        return linkedDisplays;
    }

    int getNumTracks() { return mediaDisplays.size(); }

    bool isInputWidget() { return (displayMode == DisplayMode::Input) || isHybridWidget(); }
    bool isOutputWidget() { return (displayMode == DisplayMode::Output) || isHybridWidget(); }
    bool isHybridWidget() { return displayMode == DisplayMode::Hybrid; }
    bool isThumbnailWidget() { return displayMode == DisplayMode::Thumbnail; }

    bool isInterestedInFileDrag(const StringArray& /*files*/) override
    {
        return isThumbnailWidget();
    }

    void setFixedTotalDimensions(int totalWidth, int totalHeight)
    {
        fixedTotalWidth = totalWidth;
        minTotalHeight = totalHeight;

        resized();
    }

    void resetState()
    {
        for (auto& m : mediaDisplays)
        {
            m->removeChangeListener(this);
            removeChildComponent(m.get());
        }

        mediaDisplays.clear();
    }

    void addTrackFromComponentInfo(TrackComponentInfo* trackInfo, bool fromDAW = false)
    {
        std::unique_ptr<MediaDisplayComponent> m;

        std::string label =
            trackInfo->label.empty() ? "Track-" + std::to_string(getNumTracks()) : trackInfo->label;

        if (auto audioTrackInfo = dynamic_cast<AudioTrackComponentInfo*>(trackInfo))
        {
            m = std::make_unique<AudioDisplayComponent>(
                label, audioTrackInfo->required, fromDAW, displayMode);
        }
        else if (auto midiTrackInfo = dynamic_cast<MidiTrackComponentInfo*>(trackInfo))
        {
            m = std::make_unique<MidiDisplayComponent>(
                label, midiTrackInfo->required, fromDAW, displayMode);
        }
        else
        {
            DBG_AND_LOG(
                "TrackAreaWidget::addTrackFromComponentInfo: Invalid ComponentInfo received.");
        }

        if (m)
        {
            m->setTrackID(trackInfo->id);

            if (! trackInfo->info.empty())
            {
                m->setMediaInstructions(trackInfo->info);
            }

            //m->setDisplayID(trackInfo->id);
            m->addChangeListener(this);
            addAndMakeVisible(m.get());
            mediaDisplays.push_back(std::move(m));

            resized();

            if (isThumbnailWidget())
            {
                mediaDisplays.back()->selectTrack();
            }
        }
    }

    void updateTracks(const ModelComponentInfoList& trackComponents)
    {
        resetState();

        for (const auto& info : trackComponents)
        {
            if (auto* trackInfo = dynamic_cast<TrackComponentInfo*>(info.get()))
            {
                addTrackFromComponentInfo(trackInfo);
            }
            else
            {
                // Invalid input track
                jassertfalse;
            }
        }

        resized();
    }

    void addTrackFromFilePath(URL filePath, bool fromDAW = false)
    {
        File f = filePath.getLocalFile();

        for (auto& m : mediaDisplays)
        {
            if (m->isDuplicateFile(filePath))
            {
                m->selectTrack();

                DBG_AND_LOG(
                    "TrackAreaWidget::addTrackFromFilePath: Selecting existing track containing "
                    << f.getFullPathName() << " instead of creating new track.");

                return;
            }
        }

        String ext = f.getFileExtension();
        String label = filePath.getFileName();

        bool validExt = true;

        std::unique_ptr<TrackComponentInfo> trackInfo;

        if (AudioDisplayComponent::getSupportedExtensions().contains(ext))
        {
            auto audioTrackInfo = std::make_unique<AudioTrackComponentInfo>();

            audioTrackInfo->required = false;
            audioTrackInfo->label = label.toStdString();

            trackInfo = std::move(audioTrackInfo);
        }
        else if (MidiDisplayComponent::getSupportedExtensions().contains(ext))
        {
            auto midiTrackInfo = std::make_unique<MidiTrackComponentInfo>();

            midiTrackInfo->required = false;
            midiTrackInfo->label = label.toStdString();

            trackInfo = std::move(midiTrackInfo);
        }
        else
        {
            DBG_AND_LOG("TrackAreaWidget::addTrackFromFilePath: Tried to add file "
                        << f.getFullPathName() << " with unsupported type.");

            validExt = false;
        }

        if (validExt)
        {
            addTrackFromComponentInfo(trackInfo.get(), fromDAW);
            mediaDisplays.back()->initializeDisplay(filePath);
            mediaDisplays.back()->setTrackName(filePath.getFileName());
        }
    }

    void removeTrack(MediaDisplayComponent* mediaDisplay)
    {
        mediaDisplay->removeChangeListener(this);
        removeChildComponent(mediaDisplay);

        auto it =
            std::remove_if(mediaDisplays.begin(),
                           mediaDisplays.end(),
                           [mediaDisplay](const auto& ptr) { return ptr.get() == mediaDisplay; });
        mediaDisplays.erase(it, mediaDisplays.end());

        resized();
    }

    void filesDropped(const StringArray& files, int /*x*/, int /*y*/) override
    {
        for (String f : files)
        {
            URL droppedFilePath = URL(File(f));

            addTrackFromFilePath(droppedFilePath);
        }
    }

private:
    void changeListenerCallback(ChangeBroadcaster* source) override
    {
        if (auto sourceDisplay = dynamic_cast<MediaDisplayComponent*>(source))
        {
            bool wasTrackSelected = sourceDisplay->isCurrentlySelected();

            for (auto& m : mediaDisplays)
            {
                if (source != m.get() && wasTrackSelected)
                {
                    m->deselectTrack();
                }
            }

            sendSynchronousChangeMessage();
        }
    }

    const DisplayMode displayMode;
    const int fixedTrackHeight = 0;

    const float marginSize = 4;
    int fixedTotalWidth = 0;
    int minTotalHeight = 0;

    std::vector<std::unique_ptr<MediaDisplayComponent>> mediaDisplays;
};
