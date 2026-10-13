/**
 * @file PreviewPaneWidget.h
 * @brief Pane at the bottom of the media clipboard for previewing the selected file.
 * @author NatalieElizabeth
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../gui/MultiButton.h"
#include "../gui/ResizeEdge.h"
#include "../gui/RoundIconButton.h"

#include "../media/AudioDisplayComponent.h"
#include "../media/MediaDisplayComponent.h"
#include "../media/MidiDisplayComponent.h"

#include "ModelStyle.h"

using namespace juce;

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

        minimizeButton.setPointingUp(minimized);
        minimizeButton.setInstructions(minimized ? "Click to restore the preview pane."
                                                 : "Click to minimize the preview pane.");
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
        if (AudioDisplayComponent::getSupportedExtensions().contains(extension, true))
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
        else if (MidiDisplayComponent::getSupportedExtensions().contains(extension, true))
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

    // Stops playback and returns to the start, keeping the track shown
    void stopPlayback()
    {
        if (currentDisplay != nullptr)
        {
            currentDisplay->stop();
        }
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
        Rectangle<int> contentArea = getContentArea();

        // Headed like a section of a model tab, with the buttons at the end of its rule
        ModelStyle::drawSectionHeader(
            g, "Preview", getTitleArea().withTrimmedRight(getButtonsWidth() + buttonGroupGap));

        // Behind the display, like a track, or in its place until there is one to show
        g.setColour(Colours::darkgrey);
        g.fillRect(contentArea);

        if (currentDisplay == nullptr)
        {
            g.setColour(ModelStyle::secondaryText);
            g.setFont(ModelStyle::font(13.0f));
            g.drawText("No track selected.", contentArea, Justification::centred);
        }
    }

    void resized() override
    {
        // A minimized pane cannot be resized, and shows nothing below its title bar
        resizeEdge.setBounds(0, 0, getWidth(), minimized ? 0 : resizeEdgeHeight);

        const Rectangle<int> contentArea = getContentArea();

        // Buttons sit at the right end of the title bar
        FlexBox buttonsFlexBox;
        buttonsFlexBox.flexDirection = FlexBox::Direction::row;
        buttonsFlexBox.justifyContent = FlexBox::JustifyContent::flexEnd;
        buttonsFlexBox.alignItems = FlexBox::AlignItems::center;

        for (Component* button : std::initializer_list<Component*> {
                 &playPauseButton, &stopButton, &minimizeButton, &closeButton })
        {
            // The pane's own buttons are round, like a tab's, and set apart from playback
            const bool isPaneButton = button == &minimizeButton || button == &closeButton;
            const float size = (float) (isPaneButton ? paneButtonSize : playbackButtonSize);
            const float leftMargin = button == &minimizeButton ? (float) buttonGroupGap : 0.0f;

            buttonsFlexBox.items.add(FlexItem(*button).withWidth(size).withHeight(size).withMargin(
                { 0, (float) buttonMargin, 0, leftMargin }));
        }

        buttonsFlexBox.performLayout(getTitleArea());

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
    static constexpr int titleBarHeight = ModelStyle::sectionHeaderHeight;

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
        playButtonInactiveInfo = MultiButton::Mode {
            "Play-Inactive",    "Nothing to play.", [] {}, MultiButton::DrawingMode::IconOnly,
            Colours::lightgrey, fontaudio::Play
        };
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
                                [this] { stopPlayback(); },
                                MultiButton::DrawingMode::IconOnly,
                                Colours::orangered,
                                fontaudio::Stop };
        // Mode when there is nothing to stop (stop disabled)
        stopButtonInactiveInfo = MultiButton::Mode {
            "Stop-Inactive",    "Nothing to stop.", [] {}, MultiButton::DrawingMode::IconOnly,
            Colours::lightgrey, fontaudio::Stop
        };
        stopButton.addMode(stopButtonActiveInfo);
        stopButton.addMode(stopButtonInactiveInfo);
        addAndMakeVisible(stopButton);

        minimizeButton.onClick = [this] { setMinimized(! minimized); };
        addAndMakeVisible(minimizeButton);

        closeButton.onClick = [this]
        {
            if (onClose)
            {
                onClose();
            }
        };
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

    Rectangle<int> getTitleArea() const { return getLocalBounds().removeFromTop(titleBarHeight); }

    // Below the title bar, with a gap like the one under a model tab's section header
    Rectangle<int> getContentArea() const
    {
        if (minimized)
        {
            return {};
        }

        return getLocalBounds().withTrimmedTop(titleBarHeight + contentGap);
    }

    static constexpr int getButtonsWidth()
    {
        return 2 * (playbackButtonSize + buttonMargin) + 2 * (paneButtonSize + buttonMargin)
               + buttonGroupGap;
    }

    static constexpr int resizeEdgeHeight = 4;
    static constexpr int contentGap = 2;
    static constexpr int playbackButtonSize = 24;
    static constexpr int paneButtonSize = 18;
    static constexpr int buttonMargin = 2;
    static constexpr int buttonGroupGap = 6;

    MultiButton playPauseButton;
    MultiButton::Mode playButtonActiveInfo;
    MultiButton::Mode playButtonInactiveInfo;
    MultiButton::Mode pauseButtonInfo;

    MultiButton stopButton;
    MultiButton::Mode stopButtonActiveInfo;
    MultiButton::Mode stopButtonInactiveInfo;

    ChevronButton minimizeButton { "Click to minimize the preview pane." };
    CloseButton closeButton {
        "Click to close the preview pane. It can be shown again from the View menu."
    };

    // Declared after the buttons, since their playback callbacks update the buttons
    std::unique_ptr<AudioDisplayComponent> audioDisplay;
    std::unique_ptr<MidiDisplayComponent> midiDisplay;

    // Whichever of the two displays is showing a track, or nullptr when the pane is empty
    MediaDisplayComponent* currentDisplay = nullptr;

    ResizeEdge resizeEdge { ResizeEdge::Side::Top };
    int heightAtDragStart = defaultHeight;

    bool minimized = false;
};
