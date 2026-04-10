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
    PreviewPaneWidget() {}

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

    void paint(Graphics& g) override
    {
        g.fillAll(Colour(Colours::darkgrey));

        if (currentDisplay == nullptr)
        {
            g.setColour(Colours::grey);
            g.drawText("No track selected.", getLocalBounds(), Justification::centred);
        }
    }

    void resized() override
    {
        if (currentDisplay != nullptr)
            currentDisplay->setBounds(getLocalBounds().reduced(8,0));
    }

private:
    std::unique_ptr<MediaDisplayComponent> currentDisplay;
};