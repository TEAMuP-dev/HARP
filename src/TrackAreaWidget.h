/**
 * @file TrackAreaWidget.h
 * @brief The component that displays the input and output tracks in the GUI.
 * @author xribene
 * 
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

    void setModel(std::shared_ptr<WebModel> model) { mModel = model; }

    void populateTracks()
    {
        // headerLabel.setText("No model loaded", juce::dontSendNotification);
        // headerLabel.setJustificationType(juce::Justification::centred);
        // addAndMakeVisible(headerLabel);

        if (mModel == nullptr)
        {
            DBG("populate gui called, but model is null");
            return;
        }

        auto& inputTracksInfo = mModel->getInputTracks();
        // clear the m_ctrls vector
        // m_ctrls.clear();
        // juce::Array<juce::var>& inputComponents = mModel->getControls();

        // // iterate through the list of input components
        // // and choosing only the ones that correspond to input controls and not input media
        inputTracksCounter = 0;
        // auto counter = 1;
        for (const auto& pair : inputTracksInfo)
        {
            inputTracksCounter++;
            auto trackInfo = pair.second;
            if (auto audioTrackInfo = dynamic_cast<AudioTrackInfo*>(trackInfo.get()))
            {
                if (audioTrackInfo->label == "")
                {
                    audioTrackInfo->label = "InputAudio-" + std::to_string(inputTracksCounter);
                }
                inputMediaDisplays.push_back(
                    std::make_unique<AudioDisplayComponent>(audioTrackInfo->label));
                addAndMakeVisible(inputMediaDisplays.back().get());
                inputMediaDisplays.back()->addChangeListener(this);
            }
            else if (auto midiTrackInfo = dynamic_cast<MidiTrackInfo*>(trackInfo.get()))
            {
                if (midiTrackInfo->label == "")
                {
                    midiTrackInfo->label = "InputMidi-" + std::to_string(inputTracksCounter);
                }
                inputMediaDisplays.push_back(
                    std::make_unique<MidiDisplayComponent>(midiTrackInfo->label));
                addAndMakeVisible(inputMediaDisplays.back().get());
                inputMediaDisplays.back()->addChangeListener(this);
            }
            
        }

        // if (counter)
        auto& outputTracksInfo = mModel->getOutputTracks();
        outputTracksCounter = 0;
        for (const auto& pair : outputTracksInfo)
        {
            outputTracksCounter++;
            auto trackInfo = pair.second;
            if (auto audioTrackInfo = dynamic_cast<AudioTrackInfo*>(trackInfo.get()))
            {
                if (audioTrackInfo->label == "")
                {
                    audioTrackInfo->label = "OutputAudio-" + std::to_string(outputTracksCounter);
                }
                outputMediaDisplays.push_back(
                    std::make_unique<AudioDisplayComponent>(audioTrackInfo->label));
                addAndMakeVisible(outputMediaDisplays.back().get());
                outputMediaDisplays.back()->addChangeListener(this);
            }
            else if (auto midiTrackInfo = dynamic_cast<MidiTrackInfo*>(trackInfo.get()))
            {
                if (midiTrackInfo->label == "")
                {
                    midiTrackInfo->label = "OutputMidi-" + std::to_string(outputTracksCounter);
                }
                outputMediaDisplays.push_back(
                    std::make_unique<MidiDisplayComponent>(midiTrackInfo->label));
                addAndMakeVisible(outputMediaDisplays.back().get());
                outputMediaDisplays.back()->addChangeListener(this);
            }
        }

        if (inputTracksCounter > 0)
        {
            // inputTracksLabel->setText("Input Tracks", juce::dontSendNotification);
            inputTracksLabel->setJustificationType(juce::Justification::centred);
            inputTracksLabel->setFont(juce::Font(20.0f, juce::Font::bold));
            addAndMakeVisible(inputTracksLabel.get());
        }
        
        if (outputTracksCounter > 0)
        {
            // outputTracksLabel->setText("Output Tracks", juce::dontSendNotification);
            outputTracksLabel->setJustificationType(juce::Justification::centred);
            outputTracksLabel->setFont(juce::Font(20.0f, juce::Font::bold));
            addAndMakeVisible(outputTracksLabel.get());
        }

        repaint();
        resized();
    }

    void resetUI()
    {
        DBG("ControlAreaWidget::resetUI called");
        mModel.reset();

        // TODO: do I need this  ?
        // TODO: also, does this need to go to the destructor ?
        for (auto& inputMediaDisplay : inputMediaDisplays)
        {
            inputMediaDisplay->removeChangeListener(this);
            removeChildComponent(inputMediaDisplay.get());
        }

        for (auto& outputMediaDisplay : outputMediaDisplays)
        {
            outputMediaDisplay->removeChangeListener(this);
            removeChildComponent(outputMediaDisplay.get());
        }

        inputMediaDisplays.clear();
        outputMediaDisplays.clear();

        inputTracksCounter = 0;
        outputTracksCounter = 0;
        // remove input and output TracksLabel
        // inputTracksLabel->setText("", juce::dontSendNotification);
        // outputTracksLabel->setText("", juce::dontSendNotification);
    }

    void resized() override
    {
        auto area = getLocalBounds();

        // headerLabel.setBounds(area.removeFromTop(30));  // Adjust height to your preference

        juce::FlexBox mainBox;
        mainBox.flexDirection =
            juce::FlexBox::Direction::column; // Set the main flex direction to column

        juce::FlexItem::Margin margin(2);

        if (inputTracksCounter > 0)
        {
            // Before input tracks, add a centered label for the input tracks
            mainBox.items.add(juce::FlexItem(*inputTracksLabel).withFlex(0.25).withMinHeight(15));

            // Input tracks
            for (auto& display : inputMediaDisplays)
            {
                // auto row = area.removeFromTop(150).reduced(2);
                // display->setBounds(row);
                mainBox.items.add(juce::FlexItem(*display).withFlex(1).withMinHeight(50).withMargin(4));
            }
        }

        if (outputTracksCounter > 0)
        {
            // Before output tracks, add a centered label for the output tracks
            mainBox.items.add(juce::FlexItem(*outputTracksLabel).withFlex(0.25).withMinHeight(15));

            // Output tracks
            for (auto& display : outputMediaDisplays)
            {
                // auto row = area.removeFromTop(150).reduced(2);
                // display->setBounds(row);
                mainBox.items.add(juce::FlexItem(*display).withFlex(1).withMinHeight(50).withMargin(4));
            }
        }

        // Perform Layout
        mainBox.performLayout(area);
    }

    // Getter for inputMediaDisplays
    std::vector<std::unique_ptr<MediaDisplayComponent>>& getInputMediaDisplays()
    {
        return inputMediaDisplays;
    }

    // Implement the listener callback
    // It listens to events emmited from MediaDisplayComponents 
    // using the sendChangeMessage() function
    // NOTE: not used, but might be useful in the future
    void changeListenerCallback(juce::ChangeBroadcaster* source) override
    {
        // Check if the source is one of the inputMediaDisplays
        for (auto& display : inputMediaDisplays)
        {
            if (source == display.get())
            {
                // Do something
                return;
            }
        }
        for (auto& display : outputMediaDisplays)
        {
            if (source == display.get())
            {
                // Do something
                return;
            }
        }
    }

private:

    // ToolbarSliderStyle toolbarSliderStyle;
    std::shared_ptr<WebModel> mModel { nullptr };

    juce::Label headerLabel;
    // HARPLookAndFeel mHARPLookAndFeel;

    // A list of input media displays
    std::vector<std::unique_ptr<MediaDisplayComponent>> inputMediaDisplays;
    // A list of output media displays
    std::vector<std::unique_ptr<MediaDisplayComponent>> outputMediaDisplays;

    // A label for the input tracks
    std::unique_ptr<juce::Label> inputTracksLabel { std::make_unique<juce::Label>("Input Tracks",
                                                                                  "Input Tracks") };
    // A label for the output tracks
    std::unique_ptr<juce::Label> outputTracksLabel {
        std::make_unique<juce::Label>("Output Tracks", "Output Tracks")
    };
    int inputTracksCounter = 0;
    int outputTracksCounter = 0;
};
