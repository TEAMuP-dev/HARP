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

    bool isInputWidget() { return (displayMode == 0) || isHybridWidget(); }
    bool isOutputWidget() { return (displayMode == 1) || isHybridWidget(); }
    bool isHybridWidget() { return displayMode == 2; }
    bool isThumbnailWidget() { return displayMode == 3; }

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

    void resetUI()
    {
        // TODO - is this necessary?
        // TODO - does this need to go in the destructor?

        for (auto& m : mediaDisplays)
        {
            m->removeChangeListener(this);
            removeChildComponent(m.get());
        }

        mediaDisplays.clear();
    }

    void addTrackFromComponentInfo(ComponentInfo info, bool fromDAW = false)
    {
        std::shared_ptr<PyHarpComponentInfo> trackInfo = info.second;
        std::unique_ptr<MediaDisplayComponent> m;

        std::string label =
            trackInfo->label.empty() ? "Track-" + std::to_string(getNumTracks()) : trackInfo->label;

        if (auto audioTrackInfo = dynamic_cast<AudioTrackInfo*>(trackInfo.get()))
        {
            m = std::make_unique<AudioDisplayComponent>(
                label, audioTrackInfo->required, fromDAW, displayMode);
        }
        else if (auto midiTrackInfo = dynamic_cast<MidiTrackInfo*>(trackInfo.get()))
        {
            m = std::make_unique<MidiDisplayComponent>(
                label, midiTrackInfo->required, fromDAW, displayMode);
        }
        else
        {
            DBG("TrackAreaWidget::addTrackFromComponentInfo: Invalid ComponentInfo received.");
        }

        if (m)
        {
            m->setDisplayID(trackInfo->id);
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

    void addTrackFromFilePath(URL filePath, bool fromDAW = false)
    {
        File f = filePath.getLocalFile();

        for (auto& m : mediaDisplays)
        {
            if (m->isDuplicateFile(filePath))
            {
                m->selectTrack();

                DBG("TrackAreaWidget::addTrackFromFilePath: Selecting existing track containing "
                    << f.getFullPathName() << " instead of creating new track.");

                return;
            }
        }

        String ext = f.getFileExtension();
        String label = filePath.getFileName();

        bool validExt = true;

        ComponentInfo componentInfo;

        if (AudioDisplayComponent::getSupportedExtensions().contains(ext))
        {
            auto audioTrackInfo = std::make_shared<AudioTrackInfo>();
            audioTrackInfo->id = Uuid();
            audioTrackInfo->required = false;
            audioTrackInfo->label = label.toStdString();
            componentInfo = ComponentInfo(audioTrackInfo->id, audioTrackInfo);
        }
        else if (MidiDisplayComponent::getSupportedExtensions().contains(ext))
        {
            auto midiTrackInfo = std::make_shared<MidiTrackInfo>();
            midiTrackInfo->id = Uuid();
            midiTrackInfo->required = false;
            midiTrackInfo->label = label.toStdString();
            componentInfo = ComponentInfo(midiTrackInfo->id, midiTrackInfo);
        }
        else
        {
            DBG("TrackAreaWidget::addTrackFromFilePath: Tried to add file "
                << f.getFullPathName() << " with unsupported type.");

            validExt = false;
        }

        if (validExt)
        {
            addTrackFromComponentInfo(componentInfo, fromDAW);
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
