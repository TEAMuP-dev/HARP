/**
 * @file PreviewPaneWidget.h
 * @brief Pane at the bottom of the media clipboard for previewing the selected file.
 * @author NatalieElizabeth
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../media/AudioDisplayComponent.h"
#include "../media/MediaDisplayComponent.h"
#include "../media/MidiDisplayComponent.h"

#include "../utils/Interface.h"

using namespace juce;

class PreviewPaneWidget : public Component
{
public:
    /**
     * Shows a media file in the pane, replacing whatever was shown before. A file
     * that is neither audio nor MIDI leaves the pane empty.
     */
    void showTrack(const URL& filePath)
    {
        clearTrack();

        const String extension = filePath.getLocalFile().getFileExtension();

        // Each display is created the first time it is needed, then kept and reused
        if (AudioDisplayComponent::getSupportedExtensions().contains(extension))
        {
            if (audioDisplay == nullptr)
            {
                audioDisplay = std::make_unique<AudioDisplayComponent>(
                    "Preview", false, false, DisplayMode::Preview);
                addAndMakeVisible(*audioDisplay);
            }

            currentDisplay = audioDisplay.get();
        }
        else if (MidiDisplayComponent::getSupportedExtensions().contains(extension))
        {
            if (midiDisplay == nullptr)
            {
                midiDisplay = std::make_unique<MidiDisplayComponent>(
                    "Preview", false, false, DisplayMode::Preview);
                addAndMakeVisible(*midiDisplay);
            }

            currentDisplay = midiDisplay.get();
        }

        if (currentDisplay != nullptr)
        {
            currentDisplay->initializeDisplay(filePath);
        }

        resized();
        repaint();
    }

    // Stops playback and empties the pane
    void clearTrack()
    {
        if (currentDisplay != nullptr)
        {
            currentDisplay->stop();
            currentDisplay->resetDisplay();
            currentDisplay = nullptr;
        }

        resized();
        repaint();
    }

    void paint(Graphics& g) override
    {
        Rectangle<int> contentArea = getLocalBounds();
        Rectangle<int> titleArea = contentArea.removeFromTop(titleBarHeight);

        // Title bar
        g.setColour(
            getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
        g.fillRect(titleArea);

        g.setColour(Colours::lightgrey);
        g.setFont(Font(FontOptions(12.0f, Font::bold)));
        g.drawText("Preview", titleArea.reduced(titlePadding, 0), Justification::centredLeft);

        // Content area
        g.setColour(Colours::darkgrey);
        g.fillRect(contentArea);

        if (currentDisplay == nullptr)
        {
            // Placeholder shown until a track is displayed
            g.setColour(Colours::grey);
            g.setFont(Font(FontOptions(14.0f)));
            g.drawText("No track selected.", contentArea, Justification::centred);
        }
    }

    void resized() override
    {
        Rectangle<int> contentArea = getLocalBounds().withTrimmedTop(titleBarHeight);

        // Only the display in use is given any space
        if (audioDisplay != nullptr)
        {
            audioDisplay->setBounds(currentDisplay == audioDisplay.get() ? contentArea
                                                                         : Rectangle<int>());
        }

        if (midiDisplay != nullptr)
        {
            midiDisplay->setBounds(currentDisplay == midiDisplay.get() ? contentArea
                                                                       : Rectangle<int>());
        }
    }

    static constexpr int defaultHeight = 150;

private:
    static constexpr int titleBarHeight = 22;
    static constexpr int titlePadding = 8;

    std::unique_ptr<AudioDisplayComponent> audioDisplay;
    std::unique_ptr<MidiDisplayComponent> midiDisplay;

    // Whichever of the two displays is showing a track, or nullptr when the pane is empty
    MediaDisplayComponent* currentDisplay = nullptr;
};
