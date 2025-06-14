/*
 * @file MediaClipboardWidget.h
 * @brief The component that manages cached files HARP.
 * @author cwitkowitz
 */

#pragma once
#include "gui/MultiButton.h"
#include "juce_gui_basics/juce_gui_basics.h"

class MediaClipboardWidget : public juce::Component
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
            juce::Colours::green,
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
            juce::Colours::green,
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
            juce::Colours::lightgrey,
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
        addAndMakeVisible(trackAreaPlaceholder);
    }

    ~MediaClipboardWidget() {}

    void paint(Graphics& g) { g.fillAll(Colours::lightgrey.darker().withAlpha(0.5f)); }

    void resized() override
    {
        auto mainArea = getLocalBounds();

        juce::FlexBox mainBox;
        mainBox.flexDirection = juce::FlexBox::Direction::column;

        juce::FlexItem::Margin margin(2);

        juce::FlexBox controlBar;
        controlBar.flexDirection = juce::FlexBox::Direction::row;

        controlBar.items.add(juce::FlexItem(selectedFileLabel).withFlex(10).withMargin(margin));
        controlBar.items.add(juce::FlexItem(addFileButton).withFlex(1).withMargin(margin));
        controlBar.items.add(juce::FlexItem(removeFileButton).withFlex(1).withMargin(margin));
        mainBox.items.add(juce::FlexItem(controlBar).withHeight(30));

        mainBox.items.add(juce::FlexItem(trackAreaPlaceholder).withFlex(1).withMargin(margin));

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

    // TODO - TrackAreaWidget
    Component trackAreaPlaceholder;

    int selectedIdx = -1;
};
