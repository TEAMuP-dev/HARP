/*
 * @file TrackAreaWidget.h
 * @brief The component that displays a group of tracks in the GUI.
 * @author xribene, cwitkowitz
 */

#pragma once

#include "WebModel.h"
#include "gui/SliderWithLabel.h"
#include "gui/TitledTextBox.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "media/AudioDisplayComponent.h"
#include "media/MediaDisplayComponent.h"
#include "media/MidiDisplayComponent.h"
#include "utils.h"

using namespace juce;

class TrackAreaWidget : public Component, public ChangeListener, public FileDragAndDropTarget
{
public:
    bool isInputWidget() { return (displayMode == 0) || isHybridWidget(); }
    bool isOutputWidget() { return (displayMode == 1) || isHybridWidget(); }
    bool isHybridWidget() { return displayMode == 2; }
    bool isThumbnailWidget() { return displayMode == 3; }

    TrackAreaWidget(DisplayMode mode = DisplayMode::Input, int height = 0)
        : displayMode(mode), fixedHeight(height)
    {
    }

    void addTrack(ComponentInfo info)
    {
        std::shared_ptr<PyHarpComponentInfo> trackInfo = info.second;
        std::unique_ptr<MediaDisplayComponent> m;

        std::string label =
            trackInfo->label.empty() ? "Track-" + std::to_string(getNumTracks()) : trackInfo->label;

        if (auto audioTrackInfo = dynamic_cast<AudioTrackInfo*>(trackInfo.get()))
        {
            m = std::make_unique<AudioDisplayComponent>(label, audioTrackInfo->required, displayMode);
        }
        else if (auto midiTrackInfo = dynamic_cast<MidiTrackInfo*>(trackInfo.get()))
        {
            m = std::make_unique<MidiDisplayComponent>(label, midiTrackInfo->required, displayMode);
        }

        if (m)
        {
            m->setTrackId(trackInfo->id);
            m->addChangeListener(this);
            m->instructionBoxWriter = [this](const String& text) { updateInstructionBox(text); };
            addAndMakeVisible(m.get());
            mediaDisplays.push_back(std::move(m));
        }

        repaint();
        resized();
    }

    bool isInterestedInFileDrag(const StringArray& /*files*/) override { return isThumbnailWidget(); }

    void filesDropped(const StringArray& files, int /*x*/, int /*y*/)
    {
        for (String f : files)
        {
            File droppedFile = File(f);
            String ext = droppedFile.getFileExtension();

            if (AudioDisplayComponent::getSupportedExtensions().contains(ext))
            {
                auto audioTrackInfo = std::make_shared<AudioTrackInfo>();
                audioTrackInfo->id = Uuid();
                audioTrackInfo->required = false;
                ComponentInfo componentInfo { audioTrackInfo->id, audioTrackInfo };
                addTrack(componentInfo);
                mediaDisplays.back()->initializeDisplay(URL(droppedFile));
            }
            else if (MidiDisplayComponent::getSupportedExtensions().contains(ext))
            {
                auto midiTrackInfo = std::make_shared<MidiTrackInfo>();
                midiTrackInfo->id = Uuid();
                midiTrackInfo->required = false;
                ComponentInfo componentInfo { midiTrackInfo->id, midiTrackInfo };
                addTrack(componentInfo);
                mediaDisplays.back()->initializeDisplay(URL(droppedFile));
            }
            else
            {
                DBG("Attempted adding track of unsupported file type: " << f);
            }
        }
    }

    void resetUI()
    {
        DBG("TrackAreaWidget::resetUI called");

        // TODO: do I need this  ?
        // TODO: also, does this need to go to the destructor ?
        for (auto& m : mediaDisplays)
        {
            m->removeChangeListener(this);
            removeChildComponent(m.get());
        }

        mediaDisplays.clear();
    }

    void paint(Graphics& g) override
    {
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized() override
    {
        auto area = getLocalBounds();

        FlexBox mainBox;
        mainBox.flexDirection = FlexBox::Direction::column; // Set the main flex direction to column

        FlexItem::Margin margin(2);

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

        mainBox.performLayout(area);
    }

    // Implement the listener callback
    // It listens to events emmited from MediaDisplayComponents
    // using the sendChangeMessage() function
    // NOTE: not used, but might be useful in the future
    void changeListenerCallback(ChangeBroadcaster* source) override
    {
        for (auto& m : mediaDisplays)
        {
            if (source == m.get())
            {
                return;
            }
        }
    }

    std::vector<std::unique_ptr<MediaDisplayComponent>>& getMediaDisplays()
    {
        return mediaDisplays;
    }

    int getNumTracks() { return mediaDisplays.size(); }

private:
    void updateInstructionBox(const String& text)
    {
        if (instructionBox != nullptr)
        {
            instructionBox->setStatusMessage(text);
        }
    }

    std::vector<std::unique_ptr<MediaDisplayComponent>> mediaDisplays;

    SharedResourcePointer<InstructionBox> instructionBox;

    const DisplayMode displayMode;
    const int fixedHeight = 0;
};
