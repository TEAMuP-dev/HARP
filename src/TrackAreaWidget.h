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
//   public Button::Listener,
//   public Slider::Listener,
//   public ComboBox::Listener,
//   public TextEditor::Listener
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
        auto counter = 1;
        for (const auto& pair : inputTracksInfo)
        {
            auto trackInfo = pair.second;
            if (auto audioTrackInfo = dynamic_cast<AudioTrackInfo*>(trackInfo.get()))
            {
                if (audioTrackInfo->label == "")
                {
                    audioTrackInfo->label = "InputAudio-" + std::to_string(counter);
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
                    midiTrackInfo->label = "InputMidi-" + std::to_string(counter);
                }
                inputMediaDisplays.push_back(
                    std::make_unique<MidiDisplayComponent>(midiTrackInfo->label));
                addAndMakeVisible(inputMediaDisplays.back().get());
                inputMediaDisplays.back()->addChangeListener(this);
            }
            counter++;
        }

        auto& outputTracksInfo = mModel->getOutputTracks();
        counter = 1;
        for (const auto& pair : outputTracksInfo)
        {
            auto trackInfo = pair.second;
            if (auto audioTrackInfo = dynamic_cast<AudioTrackInfo*>(trackInfo.get()))
            {
                if (audioTrackInfo->label == "")
                {
                    audioTrackInfo->label = "OutputAudio-" + std::to_string(counter);
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
                    midiTrackInfo->label = "OutputMidi-" + std::to_string(counter);
                }
                outputMediaDisplays.push_back(
                    std::make_unique<MidiDisplayComponent>(midiTrackInfo->label));
                addAndMakeVisible(outputMediaDisplays.back().get());
                outputMediaDisplays.back()->addChangeListener(this);
            }
        }

        // inputTracksLabel->setText("Input Tracks", juce::dontSendNotification);
        inputTracksLabel->setJustificationType(juce::Justification::centred);
        inputTracksLabel->setFont(juce::Font(20.0f, juce::Font::bold));
        addAndMakeVisible(inputTracksLabel.get());

        // outputTracksLabel->setText("Output Tracks", juce::dontSendNotification);
        outputTracksLabel->setJustificationType(juce::Justification::centred);
        outputTracksLabel->setFont(juce::Font(20.0f, juce::Font::bold));
        addAndMakeVisible(outputTracksLabel.get());

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
    }

    void resized() override
    {
        auto area = getLocalBounds();

        // headerLabel.setBounds(area.removeFromTop(30));  // Adjust height to your preference

        juce::FlexBox mainBox;
        mainBox.flexDirection =
            juce::FlexBox::Direction::column; // Set the main flex direction to column

        juce::FlexItem::Margin margin(2);

        // Before input tracks, add a centered label for the input tracks
        mainBox.items.add(juce::FlexItem(*inputTracksLabel).withFlex(1).withMinHeight(30));

        // Input tracks
        for (auto& display : inputMediaDisplays)
        {
            // auto row = area.removeFromTop(150).reduced(2);
            // display->setBounds(row);
            mainBox.items.add(juce::FlexItem(*display).withFlex(1).withMinHeight(50));
        }

        // Before output tracks, add a centered label for the output tracks
        mainBox.items.add(juce::FlexItem(*outputTracksLabel).withFlex(1).withMinHeight(30));

        // Output tracks
        for (auto& display : outputMediaDisplays)
        {
            // auto row = area.removeFromTop(150).reduced(2);
            // display->setBounds(row);
            mainBox.items.add(juce::FlexItem(*display).withFlex(1).withMinHeight(50));
        }

        // Perform Layout
        mainBox.performLayout(area);
    }

    // Getter for inputMediaDisplays
    std::vector<std::unique_ptr<MediaDisplayComponent>>& getInputMediaDisplays()
    {
        return inputMediaDisplays;
    }

    // TODO: a function for drag'n'drop a file from one track to another

    // Implement the listener callback
    void changeListenerCallback(juce::ChangeBroadcaster* source) override
    {
        // Check if the source is one of the inputMediaDisplays
        for (auto& display : inputMediaDisplays)
        {
            if (source == display.get())
            {
                if (display->isFileDropped())
                {
                    URL droppedFilePath = display->getDroppedFilePath();
                    display->clearDroppedFile();
                    // Reload an appropriate display for dropped file
                    loadMediaDisplay(droppedFilePath.getLocalFile(), display);
                    // File mediaFile = droppedFilePath.getLocalFile();
                    // String extension = mediaFile.getFileExtension();
                }
                // else if (display->isFileLoaded() && !display->isPlaying())
                // {
                //     playStopButton.setMode(playButtonInfo.label);
                //     playStopButton.setEnabled(true);
                // }
                // else if (display->isFileLoaded() && display->isPlaying())
                // {
                //     playStopButton.setMode(stopButtonInfo.label);
                // }
                // else
                // {
                //     playStopButton.setMode(playButtonInfo.label);
                //     playStopButton.setEnabled(false);
                // }
                return;
            }
        }
    }

private:
    void loadMediaDisplay(File mediaFile, std::unique_ptr<MediaDisplayComponent>& cur_mediaDisplay)
    {
        String extension = mediaFile.getFileExtension();

        bool matchingDisplay = true;

        if (dynamic_cast<AudioDisplayComponent*>(cur_mediaDisplay.get()))
        {
            matchingDisplay = audioExtensions.contains(extension);
        }
        else
        {
            matchingDisplay = midiExtensions.contains(extension);
        }

        if (! matchingDisplay)
        {
            // AlertWindow("Error", "Unsupported file type.", AlertWindow::WarningIcon);
            AlertWindow::showMessageBoxAsync(
                AlertWindow::WarningIcon, "Save As", "Wrong FileType\n", "OK");

            // return;
            // // Remove the existing media display
            // removeChildComponent(cur_mediaDisplay.get());
            // cur_mediaDisplay->removeChangeListener(this);
            // // mediaDisplayHandler->detach();

            // int mediaType = 0;

            // if (audioExtensions.contains(extension))
            // {
            // }
            // else if (midiExtensions.contains(extension))
            // {
            //     mediaType = 1;
            // }
            // else
            // {
            //     DBG("MainComponent::loadMediaDisplay: Unsupported file type \'" << extension
            //                                                                     << "\'.");

            //     AlertWindow("Error", "Unsupported file type.", AlertWindow::WarningIcon);
            // }

            // // Initialize a matching display
            // initializeMediaDisplay(mediaType, cur_mediaDisplay);
        }
        else
        {
            cur_mediaDisplay->setupDisplay(URL(mediaFile));
            // lastLoadTime = Time::getCurrentTime();
            // playStopButton.setEnabled(true);
            resized();
        }
    }

    StringArray audioExtensions = AudioDisplayComponent::getSupportedExtensions();
    StringArray midiExtensions = MidiDisplayComponent::getSupportedExtensions();

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
};
