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

class DragOverlayComponent : public Component
{
public:
    DragOverlayComponent()
    {
        setInterceptsMouseClicks(false, false);
    }

    void startDrag(Image snapshot, Point<int> offset)
    {
        if (isActive) return;
        dragImage = snapshot;
        dragOffset = offset;
        isActive = true;
        setVisible(true);
        repaint();
    }

    // Called every mouseDrag event with the current mouse position
    void updatePosition(Point<int> newPos)
    {
        if (newPos == currentPos) return;

        // Clear old position
        repaint(currentPos.x - dragOffset.x,
                currentPos.y - dragOffset.y,
                dragImage.getWidth(),
                dragImage.getHeight());

        currentPos = newPos;

        // Repaint only new position
        repaint(currentPos.x - dragOffset.x,
                currentPos.y - dragOffset.y,
                dragImage.getWidth(),
                dragImage.getHeight());
    }

    void stopDrag()
    {
        isActive = false;
        dragImage = Image(); // Releases the image data
        setVisible(false);
        repaint();
    }

    void paint(Graphics& g) override
    {
        if (!isActive || dragImage.isNull())
            return;

        g.drawImage(dragImage,
                    currentPos.x - dragOffset.x,
                    currentPos.y - dragOffset.y,
                    dragImage.getWidth(),
                    dragImage.getHeight(),
                    0,
                    0,
                    dragImage.getWidth(),
                    dragImage.getHeight());
    }

private:
    Image dragImage;
    Point<int> currentPos;
    Point<int> dragOffset;
    bool isActive = false;
};

class TrackAreaWidget : public Component,
                        public ChangeListener,
                        public ChangeBroadcaster,
                        public FileDragAndDropTarget
{
public:
    TrackAreaWidget(DisplayMode mode = DisplayMode::Input,
                    int trackHeight = 0,
                    DragOverlayComponent* overlay = nullptr)
        : displayMode(mode), fixedTrackHeight(trackHeight), dragOverlay(overlay)
    {
        addMouseListener(this, true);
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
            int draggedIndex = isDraggingTrack ? getDraggedTrackIndex() : -1;

            int visualGapIndex = dragInsertIndex;
            if (isDraggingTrack && draggedIndex >= 0 && dragInsertIndex > draggedIndex)
                visualGapIndex = dragInsertIndex - 1;

            int layoutIndex = 0;

            for (auto& m : mediaDisplays)
            {
                // Don't draw dragged track
                if (m.get() == draggedTrack && isDraggingTrack)
                    continue;

                if (isDraggingTrack && dragInsertIndex >= 0 && layoutIndex == visualGapIndex)
                {
                    FlexItem gap;

                    if (fixedTrackHeight)
                        gap = FlexItem().withHeight(fixedTrackHeight).withMargin(marginSize);
                    else  
                        gap = FlexItem().withFlex(1).withMinHeight(50).withMargin(marginSize);

                    mainBox.items.add(gap);
                }

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

                layoutIndex++;
            }

            // If the drag gap is at the end of the list
            if (isDraggingTrack && dragInsertIndex >= 0 && layoutIndex == visualGapIndex)
            {
                FlexItem gap;

                if (fixedTrackHeight)
                    gap = FlexItem().withHeight(fixedTrackHeight).withMargin(marginSize);
                else
                    gap = FlexItem().withFlex(1).withMinHeight(50).withMargin(marginSize);

                mainBox.items.add(gap);
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

    Rectangle<int> getFirstTrackFolderButtonBounds()
    {
        if (mediaDisplays.size() > 0)
        {
            auto display = mediaDisplays[0].get();
            auto bounds = display->getChooseFileButtonBounds();

            // Convert to TrackAreaWidget coordinates
            return getLocalArea(display, bounds);
        }

        return {};
    }

    Rectangle<int> getFirstTrackPlayButtonBounds()
    {
        if (mediaDisplays.size() > 0)
        {
            auto display = mediaDisplays[0].get();
            auto bounds = display->getPlayButtonBounds();

            // Convert to TrackAreaWidget coordinates
            return getLocalArea(display, bounds);
        }

        return {};
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

    void reorderTrack(MediaDisplayComponent* draggedDisplay, int newIndex)
    {
        auto it =
            std::find_if(mediaDisplays.begin(), 
                         mediaDisplays.end(),
                         [draggedDisplay](const auto& ptr) { return ptr.get() == draggedDisplay; });
        
        if (it == mediaDisplays.end()) return;

        // Track the old index for downward reordering
        int oldIndex = std::distance(mediaDisplays.begin(), it);

        auto draggedPtr = std::move(*it);
        mediaDisplays.erase(it);

        // Decrement index if moving downward to account for shifting indicies
        if (newIndex > oldIndex)
            newIndex--;

        newIndex = jlimit(0, (int)mediaDisplays.size(), newIndex);

        mediaDisplays.insert(mediaDisplays.begin() + newIndex, std::move(draggedPtr));

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

    int getInsertIndexAtY(int y)
    {
        int trackSlotHeight = fixedTrackHeight + static_cast<int>(2 * marginSize);
        
        if (isDraggingTrack && draggedTrack != nullptr)
        {
            int draggedIndex = getDraggedTrackIndex();

            int draggedSlotTop = draggedIndex * trackSlotHeight;

            if (draggedIndex >= 0 && y > draggedSlotTop)
                y += trackSlotHeight;
        }

        int index = y / trackSlotHeight;
        return jlimit(0, getNumTracks(), index);
    }

    int getDraggedTrackIndex() const
    {
        if (draggedTrack == nullptr) return -1;

        for (int i = 0; i < (int)mediaDisplays.size(); i++)
        {
            if (mediaDisplays[i].get() == draggedTrack)
                return i;
        }

        return -1;
    }

    void mouseDown(const MouseEvent& e) override
    {
        if (!isThumbnailWidget()) return;

        // Reset drag state at the start of every click
        draggedTrack = nullptr;
        isDraggingTrack = false;

        Component* clicked = e.eventComponent;
        
        MediaDisplayComponent* clickedDisplay = nullptr;
        Component* c = clicked;
        while (c != nullptr && c != this)
        {
            if (auto* md = dynamic_cast<MediaDisplayComponent*>(c))
            {
                clickedDisplay = md;
                break;
            }
            c = c->getParentComponent();
        }

        if (clickedDisplay)
        {
            draggedTrack = clickedDisplay;
            dragOriginIndex = getDraggedTrackIndex();

            Point<int> mouseInThis = e.getEventRelativeTo(this).getPosition();
            Point<int> trackTopLeft = draggedTrack->getBounds().getTopLeft();
            dragClickOffset = mouseInThis - trackTopLeft;
        }
    }

    void mouseDrag(const MouseEvent& e) override
    {
        if (!isThumbnailWidget() || draggedTrack == nullptr) return;

        Point<int> posInThis = e.getEventRelativeTo(this).getPosition();

        if (!isDraggingTrack && e.getDistanceFromDragStart() > 5)
        {
            isDraggingTrack = true;

            // Takes a snapshot of the track at the moment dragging starts
            if (dragOverlay != nullptr)
            {
                Image snapshot = draggedTrack->createComponentSnapshot(draggedTrack->getLocalBounds());
                dragOverlay->startDrag(snapshot, dragClickOffset);
                draggedTrack->setVisible(false);
            }
        }

        if (isDraggingTrack)
        {
            int newInsertIndex = -1;

            if (getLocalBounds().contains(posInThis))
            {
                newInsertIndex = getInsertIndexAtY(posInThis.y);
            }
            else
            {
                newInsertIndex = dragOriginIndex;
            }

            if (newInsertIndex != dragInsertIndex)
            {
                dragInsertIndex = newInsertIndex;
                resized();
            }

            // Converts the position to the overlay's coordinate space
            if (dragOverlay != nullptr)
            {
                Point<int> posInOverlay = dragOverlay->getLocalPoint(this, posInThis);
                dragOverlay->updatePosition(posInOverlay);
            }
        }
    }

    void mouseUp(const MouseEvent& e) override
    {
        if (!isThumbnailWidget()) return;

        // Tracks the release position of the mouse
        Point<int> releasePos = e.getEventRelativeTo(this).getPosition();

        // Only reorders if the mouse was released inside the widget
        if (isDraggingTrack && draggedTrack != nullptr && getLocalBounds().contains(releasePos))
        {
            int currentIndex = getDraggedTrackIndex();

            if (dragInsertIndex != currentIndex)
            {
                reorderTrack(draggedTrack, dragInsertIndex);
            }
        }

        // Restore the track's appearance and stop the overlay
        if (draggedTrack != nullptr)
            draggedTrack->setVisible(true);
        if (dragOverlay != nullptr)
            dragOverlay->stopDrag();

        // Reset the drag state
        draggedTrack = nullptr;
        dragInsertIndex = -1;
        dragOriginIndex = -1;
        isDraggingTrack = false;

        resized();
    }

    const DisplayMode displayMode;
    const int fixedTrackHeight = 0;

    const float marginSize = 4;
    int fixedTotalWidth = 0;
    int minTotalHeight = 0;

    // For reordering tracks via dragging
    MediaDisplayComponent* draggedTrack = nullptr;
    DragOverlayComponent* dragOverlay = nullptr;
    int dragInsertIndex = -1;
    int dragOriginIndex = -1;
    bool isDraggingTrack = false;
    Point<int> dragClickOffset { 0, 0 };

    std::vector<std::unique_ptr<MediaDisplayComponent>> mediaDisplays;
};
