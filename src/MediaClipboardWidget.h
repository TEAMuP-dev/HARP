/*
 * @file MediaClipboardWidget.h
 * @brief The component that manages cached files HARP.
 * @author cwitkowitz
 */

#pragma once

#include "TrackAreaWidget.h"
#include "gui/MultiButton.h"
#include "juce_gui_basics/juce_gui_basics.h"

using namespace juce;

class MediaClipboardWidget : public Component
{
public:
    MediaClipboardWidget()
    {
        addFileInfo = MultiButton::Mode {
            "Add File",
            [this]
            {
                //TODO
            },
            Colours::green,
            "Click to add a file to the media clipboard",
            MultiButton::DrawingMode::IconOnly,
            fontawesome::Plus,
        };
        addFileButton.addMode(addFileInfo);
        addFileButton.setMode(addFileInfo.label);
        addFileButton.setEnabled(true);

        removeFileActiveInfo = MultiButton::Mode {
            "Remove Selected File",
            [this]
            {
                //TODO
            },
            Colours::green,
            "Click to add a file to the media clipboard",
            MultiButton::DrawingMode::IconOnly,
            fontawesome::Minus,
        };
        removeFileInactiveInfo = MultiButton::Mode {
            "Remove Selected Track",
            [this]
            {
                //TODO
            },
            Colours::lightgrey,
            "No file selected",
            MultiButton::DrawingMode::IconOnly,
            fontawesome::Minus,
        };
        removeFileButton.addMode(removeFileActiveInfo);
        removeFileButton.addMode(removeFileInactiveInfo);
        removeFileButton.setMode(removeFileInactiveInfo.label);
        removeFileButton.setEnabled(false);

        addAndMakeVisible(selectedFileLabel);
        addAndMakeVisible(addFileButton);
        addAndMakeVisible(removeFileButton);

        //trackArea.setViewedComponent(trackAreaWidget);
        addAndMakeVisible(trackAreaWidget);
    }

    ~MediaClipboardWidget() {}

    void paint(Graphics& g) { g.fillAll(Colours::lightgrey.darker().withAlpha(0.5f)); }

    void resized() override
    {
        auto mainArea = getLocalBounds();

        FlexBox mainBox;
        mainBox.flexDirection = FlexBox::Direction::column;

        FlexItem::Margin margin(2);

        FlexBox controlBar;
        controlBar.flexDirection = FlexBox::Direction::row;

        controlBar.items.add(FlexItem(selectedFileLabel).withFlex(10).withMargin(margin));
        controlBar.items.add(FlexItem(addFileButton).withFlex(1).withMargin(margin));
        controlBar.items.add(FlexItem(removeFileButton).withFlex(1).withMargin(margin));
        mainBox.items.add(FlexItem(controlBar).withHeight(30));

        mainBox.items.add(FlexItem(trackAreaWidget).withFlex(1).withMargin(margin));

        mainBox.performLayout(mainArea);
    }

private:
    TextEditor selectedFileLabel;

    MultiButton addFileButton;
    MultiButton::Mode addFileInfo;
    MultiButton removeFileButton;
    MultiButton::Mode removeFileActiveInfo;
    MultiButton::Mode removeFileInactiveInfo;
    //MultiButton playStopButton;
    //MultiButton::Mode playButtonInfo;
    //MultiButton::Mode stopButtonInfo;
    //MultiButton saveFileButton;
    //MultiButton::Mode saveButtonActiveInfo;
    //MultiButton::Mode saveButtonInactiveInfo;

    //Viewport trackArea;
    TrackAreaWidget trackAreaWidget { true, 50 };
    //TrackAreaWidget* trackAreaWidget;

    int selectedIdx = -1;
};
