/**
 * @file PreviewPaneWidget.h
 * @brief Pane at the bottom of the media clipboard for previewing the selected file.
 * @author NatalieElizabeth
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../gui/MultiButton.h"

#include "../media/AudioDisplayComponent.h"
#include "../media/MediaDisplayComponent.h"
#include "../media/MidiDisplayComponent.h"

#include "../utils/Interface.h"

using namespace juce;

//Strip along the top of the preview pane that is dragged to change the pane's height.
class PreviewPaneResizeEdge : public Component
{
public:
    PreviewPaneResizeEdge() { setMouseCursor(MouseCursor::UpDownResizeCursor); }

    // Called once when a drag begins
    std::function<void()> onDragStart;
    // Called throughout a drag with how far down the mouse is from where the drag began
    std::function<void(int)> onDrag;

    void mouseDown(const MouseEvent&) override
    {
        if (onDragStart)
        {
            onDragStart();
        }
    }

    void mouseDrag(const MouseEvent& e) override
    {
        if (onDrag)
        {
            onDrag(e.getScreenY() - e.getMouseDownScreenY());
        }
    }
};

class PreviewPaneWidget : public Component
{
public:
    PreviewPaneWidget()
    {
        initializeButtons();
        updateButtonState();

        resizeEdge.onDragStart = [this] { heightAtDragStart = getHeight(); };
        resizeEdge.onDrag = [this](int distanceDown)
        {
            if (onResize)
            {
                onResize(heightAtDragStart - distanceDown);
            }
        };
        addAndMakeVisible(resizeEdge);
    }

    // Called with the height the pane is asking for while its top edge is dragged
    std::function<void(int)> onResize;
    // Called after the pane has been minimized to its title bar, or is restored
    std::function<void()> onMinimizeChanged;
    // Called when the close button is clicked
    std::function<void()> onClose;

    bool isMinimized() const { return minimized; }

    // Playback carries on while minimized, since the title bar keeps its controls
    void setMinimized(bool shouldBeMinimized)
    {
        if (minimized == shouldBeMinimized)
        {
            return;
        }

        minimized = shouldBeMinimized;

        minimizeButton.setMode(minimized ? restoreButtonInfo.displayLabel
                                         : minimizeButtonInfo.displayLabel);
        resized();

        if (onMinimizeChanged)
        {
            onMinimizeChanged();
        }
    }

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
                audioDisplay->onPlaybackStateChanged = [this] { updateButtonState(); };
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
                midiDisplay->onPlaybackStateChanged = [this] { updateButtonState(); };
                addAndMakeVisible(*midiDisplay);
            }

            currentDisplay = midiDisplay.get();
        }

        if (currentDisplay != nullptr)
        {
            currentDisplay->initializeDisplay(filePath);
        }

        updateButtonState();
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

        updateButtonState();
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

        // Title, in the space left of the buttons
        titleArea.removeFromRight(numButtons * (buttonSize + buttonMargin) + buttonGroupGap);

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
        // A minimized pane cannot be resized, and shows nothing below its title bar
        resizeEdge.setBounds(0, 0, getWidth(), minimized ? 0 : resizeEdgeHeight);

        Rectangle<int> contentArea = minimized ? Rectangle<int>() : getLocalBounds();
        Rectangle<int> titleArea = getLocalBounds().removeFromTop(titleBarHeight);
        contentArea.removeFromTop(titleBarHeight);

        // Buttons sit at the right end of the title bar
        FlexBox buttonsFlexBox;
        buttonsFlexBox.flexDirection = FlexBox::Direction::row;
        buttonsFlexBox.justifyContent = FlexBox::JustifyContent::flexEnd;
        buttonsFlexBox.alignItems = FlexBox::AlignItems::center;

        for (MultiButton* button : { &playPauseButton, &stopButton, &minimizeButton, &closeButton })
        {
            // A wider gap sets the playback buttons apart from the pane's own buttons
            const float leftMargin = button == &minimizeButton ? buttonGroupGap : 0.0f;

            buttonsFlexBox.items.add(FlexItem(*button)
                                         .withWidth(buttonSize)
                                         .withHeight(buttonSize)
                                         .withMargin({ 0, buttonMargin, 0, leftMargin }));
        }

        buttonsFlexBox.performLayout(titleArea);

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
    static constexpr int minimumHeight = 100;
    static constexpr int titleBarHeight = 24;

private:
    void initializeButtons()
    {
        // Mode when a track is shown and is not playing (play enabled)
        playButtonActiveInfo = MultiButton::Mode { "Play-Active",
                                                   "Click to start playback.",
                                                   [this] { playCallback(); },
                                                   MultiButton::DrawingMode::IconOnly,
                                                   Colours::limegreen,
                                                   fontaudio::Play };
        // Mode when no track is shown (play disabled)
        playButtonInactiveInfo =
            MultiButton::Mode { "Play-Inactive",    "Nothing to play.",
                                [] {},              MultiButton::DrawingMode::IconOnly,
                                Colours::lightgrey, fontaudio::Play };
        // Mode during playback (pause enabled)
        pauseButtonInfo = MultiButton::Mode { "Pause",
                                              "Click to pause playback.",
                                              [this] { pauseCallback(); },
                                              MultiButton::DrawingMode::IconOnly,
                                              Colours::yellow,
                                              fontaudio::Pause };
        playPauseButton.addMode(playButtonActiveInfo);
        playPauseButton.addMode(playButtonInactiveInfo);
        playPauseButton.addMode(pauseButtonInfo);
        addAndMakeVisible(playPauseButton);

        // Mode while playing or paused (stop enabled)
        stopButtonActiveInfo =
            MultiButton::Mode { "Stop-Active",
                                "Click to stop playback and return to the start.",
                                [this] { stopCallback(); },
                                MultiButton::DrawingMode::IconOnly,
                                Colours::orangered,
                                fontaudio::Stop };
        // Mode when there is nothing to stop (stop disabled)
        stopButtonInactiveInfo =
            MultiButton::Mode { "Stop-Inactive",    "Nothing to stop.",
                                [] {},              MultiButton::DrawingMode::IconOnly,
                                Colours::lightgrey, fontaudio::Stop };
        stopButton.addMode(stopButtonActiveInfo);
        stopButton.addMode(stopButtonInactiveInfo);
        addAndMakeVisible(stopButton);

        // Mode when the pane is at its full height
        minimizeButtonInfo = MultiButton::Mode { "Minimize",
                                                 "Click to minimize the preview pane.",
                                                 [this] { setMinimized(true); },
                                                 MultiButton::DrawingMode::IconOnly,
                                                 Colours::lightgrey,
                                                 fontawesome::ChevronDown };
        // Mode when the pane is minimized to its title bar
        restoreButtonInfo = MultiButton::Mode { "Restore",
                                                "Click to restore the preview pane.",
                                                [this] { setMinimized(false); },
                                                MultiButton::DrawingMode::IconOnly,
                                                Colours::lightgrey,
                                                fontawesome::ChevronUp };
        minimizeButton.addMode(minimizeButtonInfo);
        minimizeButton.addMode(restoreButtonInfo);
        minimizeButton.setMode(minimizeButtonInfo.displayLabel);
        addAndMakeVisible(minimizeButton);

        closeButtonInfo = MultiButton::Mode {
            "Close",
            "Click to close the preview pane. It can be shown again from the View menu.",
            [this]
            {
                if (onClose)
                {
                    onClose();
                }
            },
            MultiButton::DrawingMode::IconOnly,
            Colours::lightgrey,
            fontawesome::Close
        };
        closeButton.addMode(closeButtonInfo);
        closeButton.setMode(closeButtonInfo.displayLabel);
        addAndMakeVisible(closeButton);
    }

    // Sets both buttons to match what the display is doing
    void updateButtonState()
    {
        if (currentDisplay == nullptr)
        {
            playPauseButton.setMode(playButtonInactiveInfo.displayLabel);
            stopButton.setMode(stopButtonInactiveInfo.displayLabel);

            return;
        }

        const bool playing = currentDisplay->isPlaying();
        const bool paused = currentDisplay->isPaused();

        playPauseButton.setMode(playing ? pauseButtonInfo.displayLabel
                                        : playButtonActiveInfo.displayLabel);
        stopButton.setMode(playing || paused ? stopButtonActiveInfo.displayLabel
                                             : stopButtonInactiveInfo.displayLabel);
    }

    void playCallback()
    {
        if (currentDisplay != nullptr)
        {
            currentDisplay->start();
        }
    }

    void pauseCallback()
    {
        if (currentDisplay != nullptr)
        {
            currentDisplay->pause();
        }
    }

    void stopCallback()
    {
        if (currentDisplay != nullptr)
        {
            currentDisplay->stop();
        }
    }

    static constexpr int resizeEdgeHeight = 4;
    static constexpr int titlePadding = 8;
    static constexpr int numButtons = 4;
    static constexpr int buttonSize = 20;
    static constexpr int buttonMargin = 2;
    static constexpr int buttonGroupGap = 6;

    MultiButton playPauseButton;
    MultiButton::Mode playButtonActiveInfo;
    MultiButton::Mode playButtonInactiveInfo;
    MultiButton::Mode pauseButtonInfo;

    MultiButton stopButton;
    MultiButton::Mode stopButtonActiveInfo;
    MultiButton::Mode stopButtonInactiveInfo;

    MultiButton minimizeButton;
    MultiButton::Mode minimizeButtonInfo;
    MultiButton::Mode restoreButtonInfo;

    MultiButton closeButton;
    MultiButton::Mode closeButtonInfo;

    // Declared after the buttons, since their playback callbacks update the buttons
    std::unique_ptr<AudioDisplayComponent> audioDisplay;
    std::unique_ptr<MidiDisplayComponent> midiDisplay;

    // Whichever of the two displays is showing a track, or nullptr when the pane is empty
    MediaDisplayComponent* currentDisplay = nullptr;

    PreviewPaneResizeEdge resizeEdge;
    int heightAtDragStart = defaultHeight;

    bool minimized = false;
};
