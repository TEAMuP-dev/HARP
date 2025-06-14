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

class TrackAreaWidget : public juce::Component, public juce::ChangeListener
{
public:
    TrackAreaWidget() {}

    void addTrack(ComponentInfo info)
    {
        std::shared_ptr<PyHarpComponentInfo> trackInfo = info.second;
        std::unique_ptr<MediaDisplayComponent> m;

        if (auto audioTrackInfo = dynamic_cast<AudioTrackInfo*>(trackInfo.get()))
        {
            std::string label = audioTrackInfo->label.empty()
                                    ? "Audio-" + std::to_string(getNumTracks())
                                    : audioTrackInfo->label;
            m = std::make_unique<AudioDisplayComponent>(label, audioTrackInfo->required);
        }
        else if (auto midiTrackInfo = dynamic_cast<MidiTrackInfo*>(trackInfo.get()))
        {
            std::string label = midiTrackInfo->label.empty()
                                    ? "Midi-" + std::to_string(getNumTracks())
                                    : midiTrackInfo->label;
            m = std::make_unique<MidiDisplayComponent>(label, midiTrackInfo->required);
        }

        if (m)
        {
            m->setTrackId(trackInfo->id);
            m->addChangeListener(this);
            m->instructionBoxWriter = [this](const juce::String& text)
            { updateInstructionBox(text); };
            addAndMakeVisible(m.get());
            mediaDisplays.push_back(std::move(m));
        }

        repaint();
        resized();
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

    void resized() override
    {
        auto area = getLocalBounds();

        juce::FlexBox mainBox;
        mainBox.flexDirection =
            juce::FlexBox::Direction::column; // Set the main flex direction to column

        juce::FlexItem::Margin margin(2);

        if (getNumTracks() > 0)
        {
            // Input tracks
            for (auto& m : mediaDisplays)
            {
                // auto row = area.removeFromTop(150).reduced(2);
                // display->setBounds(row);
                mainBox.items.add(juce::FlexItem(*m).withFlex(1).withMinHeight(50).withMargin(4));
            }
        }

        // Perform Layout
        mainBox.performLayout(area);
    }

    // Implement the listener callback
    // It listens to events emmited from MediaDisplayComponents
    // using the sendChangeMessage() function
    // NOTE: not used, but might be useful in the future
    void changeListenerCallback(juce::ChangeBroadcaster* source) override
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
    void updateInstructionBox(const juce::String& text)
    {
        if (instructionBox != nullptr)
        {
            instructionBox->setStatusMessage(text);
        }
    }

    std::vector<std::unique_ptr<MediaDisplayComponent>> mediaDisplays;

    juce::SharedResourcePointer<InstructionBox> instructionBox;
};
