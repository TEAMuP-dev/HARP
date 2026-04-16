/**
 * @file PreviewPaneWidget.h
 * @brief Component that allows for the previewing of MediaComponents.
 * @author NatalieElizabeth
 */

#pragma once

#include "../media/AudioDisplayComponent.h"
#include "../media/MediaDisplayComponent.h"
#include "../media/MidiDisplayComponent.h"
#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

class PreviewPaneWidget : public Component, public ChangeListener
{
public:
    PreviewPaneWidget() 
    {
        minimizeButton.setButtonText("-");
        minimizeButton.setTooltip("Minimize preview pane");
        minimizeButton.onClick = [this]
        {
            if (isMinimized)
            {
                isMinimized = false;
                minimizeButton.setButtonText("-");
                if (onResize)
                {
                    onResize(expandedHeight);
                }
            }
            else
            {
                expandedHeight = getHeight();
                isMinimized = true;

                minimizeButton.setButtonText(CharPointer_UTF8("\xe2\x96\xb2"));
                if (onResize)
                {
                    onResize(titleBarHeight);
                }
            }
        };
        addAndMakeVisible(minimizeButton);

        closeButton.setButtonText(CharPointer_UTF8("\xc3\x97"));
        closeButton.setTooltip("Close preview pane");
        closeButton.onClick = [this]
        {
            isMinimized = false;
            minimizeButton.setButtonText("-");

            if (onClose)
            {
                onClose();
            }
        };
        addAndMakeVisible(closeButton);
    }

    void showTrack(MediaDisplayComponent* source)
    {
        // Remove previosuly displayed track
        if (currentDisplay != nullptr)
        {
            currentDisplay->removeChangeListener(this);
            removeChildComponent(currentDisplay.get());
            currentDisplay.reset();
        }

        if (source == nullptr)
        {
            repaint();
            return;
        }

        URL filePath = source->getOriginalFilePath();

        if (dynamic_cast<AudioDisplayComponent*>(source) != nullptr)
        {
            auto* newDisplay =
                new AudioDisplayComponent(source->getTrackName(), false, false, DisplayMode::Preview);
            currentDisplay.reset(newDisplay);

            addAndMakeVisible(newDisplay);
            currentDisplay->addChangeListener(this);

            if (filePath.isLocalFile())
            {
                newDisplay->initializeDisplay(filePath);
                MessageManager::callAsync([this]() {
                    repaint();
                });
            }
        }
        else
        {
            auto* newDisplay =
                new MidiDisplayComponent(source->getTrackName(), false, false, DisplayMode::Preview);
            currentDisplay.reset(newDisplay);

            addAndMakeVisible(newDisplay);
            currentDisplay->addChangeListener(this);
            
            if (filePath.isLocalFile())
            {
                newDisplay->initializeDisplay(filePath);
            }
        }

        resized();
        repaint();
        MessageManager::callAsync([this]() {
            resized();
            repaint();
        });
    }

    void clearTrack()
    {
        if (currentDisplay != nullptr)
        {
            currentDisplay->removeChangeListener(this);
            removeChildComponent(currentDisplay.get());
            currentDisplay.reset();
        }
        repaint();
    }

    void changeListenerCallback(ChangeBroadcaster* source) override
    {
        DBG_AND_LOG("PreviewPaneWidget::changeListenerCallback fired");
        if (source == currentDisplay.get())
        {
            DBG_AND_LOG("PreviewPaneWidget: source matches currentDisplay, calling resized/repaint");
            resized();
            repaint();
        }
    }

    void resetMinimizeState()
    {
        isMinimized = false;
        minimizeButton.setButtonText("-");
    }

    int getExpandedHeight() const { return expandedHeight; }

    // Callbacks

    std::function<void()> onClose;

    std::function<void(int)> onResize;

    void paint(Graphics& g) override
    {
        // Widget background
        g.fillAll(Colour(Colours::darkgrey));

        // Title bar background
        Rectangle<int> titleArea(0, 0, getWidth(), titleBarHeight);
        g.setColour(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
        g.fillRect(titleArea);

        // Title bar bottom border
        g.setColour(Colours::black.withAlpha(0.4f));
        g.drawLine(0, titleBarHeight, getWidth(), titleBarHeight, 1.0f);

        // Draw "Preview" label
        g.setColour(Colours::lightgrey);
        g.setFont(Font(12.0f).boldened());
        Rectangle<int> labelArea(8, 0, getWidth() - 8 - 2 * buttonWidth - 3 * buttonMargin, titleBarHeight);
        g.drawText("Preview", labelArea, Justification::centredLeft);

        // No track loaded placeholder message
        if (currentDisplay == nullptr)
        {
            g.setColour(Colours::grey);
            g.drawText("No track selected.", getLocalBounds(), Justification::centred);
        }
    }

    void resized() override
    {
        // Position the close button flush against the right edge of the title bar
        int closeX = getWidth() - buttonMargin - buttonWidth;
        closeButton.setBounds(closeX, (titleBarHeight - buttonWidth) / 2, buttonWidth, buttonWidth);

        // Position the minimize button immediately to the left of the close button
        int minimizeX = closeX - buttonMargin - buttonWidth;
        minimizeButton.setBounds(minimizeX, (titleBarHeight - buttonWidth) / 2, buttonWidth, buttonWidth);

        if (!isMinimized && currentDisplay != nullptr)
            currentDisplay->setBounds(0, titleBarHeight, getWidth(), getHeight() - titleBarHeight);
    }

    // Mouse handling for top-edge resize

    // Called once when the user first presses the mouse button down.
    // We record the starting Y position and the starting height so we can
    // compute deltas during the drag.
    void mouseDown(const MouseEvent& e) override
    {
        if (isInResizeZone(e.y) && !isMinimized)
        {
            dragStartY = e.getScreenY();       // absolute screen Y when drag begins
            dragStartHeight = getHeight();     // height of this component at drag start
        }
    }

    // Called repeatedly as the mouse moves while a button is held down.
    // We compute how far the mouse has moved upward and ask MainComponent
    // to change the pane height accordingly.
    void mouseDrag(const MouseEvent& e) override
    {
        if (dragStartY < 0 && !isMinimized)   // drag wasn't initiated in the resize zone
            return;

        // A positive delta means the mouse moved down (pane would shrink),
        // a negative delta means the mouse moved up (pane would grow).
        int delta = e.getScreenY() - dragStartY;
        int newHeight = jmax(minimumPaneHeight, dragStartHeight - delta);

        if (onResize)
            onResize(newHeight);
    }

    // Called whenever the mouse moves (without a button held).
    // We use this to switch the cursor to a resize cursor when hovering
    // near the top edge, and back to the default otherwise.
    void mouseMove(const MouseEvent& e) override
    {
        if (isInResizeZone(e.y) && !isMinimized)
            setMouseCursor(MouseCursor::UpDownResizeCursor);
        else
            setMouseCursor(MouseCursor::NormalCursor);
    }

    // Called when the mouse button is released — reset drag tracking state.
    void mouseUp(const MouseEvent& e) override
    {
        dragStartY = -1;
        dragStartHeight = -1;

        if (dragStartY >= 0)
        {
            expandedHeight = getHeight();
        }
    }

private:
    static constexpr int titleBarHeight   = 22;   // height of the title bar strip
    static constexpr int buttonWidth      = 16;   // width and height of each title button
    static constexpr int buttonMargin     = 4;    // space between/around title buttons
    static constexpr int resizeZoneHeight = 6;    // px from top edge that trigger resize cursor
    static constexpr int minimumPaneHeight = 60;  // smallest the pane can be dragged to

    TextButton minimizeButton;
    TextButton closeButton;
    std::unique_ptr<MediaDisplayComponent> currentDisplay;

    // Negative sentinel values mean no drag is in progress
    int dragStartY      = -1;
    int dragStartHeight = -1;

    bool isMinimized = false;
    int expandedHeight = 150;

    bool isInResizeZone(int localY) const
    {
        return localY < resizeZoneHeight;
    }
};