/**
 * @file HomeTab.h
 * @brief Home tab for model discovery and loading.
 */

#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "widgets/ModelSelectionWidget.h"

using namespace juce;

class HomeTab : public Component,
                private ChangeListener
{
public:
    HomeTab()
    {
        modelSelectionWidget.addChangeListener(this);

        titleLabel.setText("Models", dontSendNotification);
        titleLabel.setJustificationType(Justification::centredLeft);
        titleLabel.setFont(Font(24.0f, Font::bold));

        subtitleLabel.setText("Select a HARP-compatible model to open it in a new tab.",
                              dontSendNotification);
        subtitleLabel.setJustificationType(Justification::centredLeft);

        addAndMakeVisible(titleLabel);
        addAndMakeVisible(subtitleLabel);
        addAndMakeVisible(modelSelectionWidget);
    }

    ~HomeTab() override
    {
        modelSelectionWidget.removeChangeListener(this);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(16);

        titleLabel.setBounds(area.removeFromTop(34));
        subtitleLabel.setBounds(area.removeFromTop(26));

        area.removeFromTop(8);
        modelSelectionWidget.setBounds(area.removeFromTop(34));
    }

    void resetSelection()
    {
        modelSelectionWidget.resetState();
    }

    Rectangle<int> getModelSelectBounds() const
    {
        return modelSelectionWidget.getBounds().expanded(2, 2);
    }

    std::function<void(String)> onModelLoadRequested;

private:
    void changeListenerCallback(ChangeBroadcaster* source) override
    {
        if (source == &modelSelectionWidget)
        {
            const auto selectedPath = modelSelectionWidget.getCurrentlySelectedPath();
            modelSelectionWidget.setDisabled();

            if (onModelLoadRequested)
                onModelLoadRequested(selectedPath);
        }
    }

    Label titleLabel;
    Label subtitleLabel;
    ModelSelectionWidget modelSelectionWidget;
};
