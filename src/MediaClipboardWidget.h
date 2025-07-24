/*
 * @file MediaClipboardWidget.h
 * @brief The component that manages cached files HARP.
 * @author cwitkowitz
 */

#pragma once

#include "TrackAreaWidget.h"
#include "gui/MultiButton.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "utils.h"

using namespace juce;

class MediaClipboardWidget : public Component, public ChangeListener
{
public:
    MediaClipboardWidget()
    {
        initializeButtons();
        resetControlState();

        controlsComponent.addAndMakeVisible(selectionTextBox);
        controlsComponent.addAndMakeVisible(buttonsComponent);
        addAndMakeVisible(controlsComponent);

        controlsFlexBox.flexDirection = FlexBox::Direction::row;
        controlsFlexBox.alignItems = FlexBox::AlignItems::stretch;

        buttonsFlexBox.flexDirection = FlexBox::Direction::row;
        buttonsFlexBox.alignItems = FlexBox::AlignItems::center;
        buttonsFlexBox.justifyContent = FlexBox::JustifyContent::center;

        trackAreaWidget.addChangeListener(this);
        //trackArea.setViewedComponent(trackAreaWidget);
        addAndMakeVisible(trackAreaWidget);

        mainFlexBox.flexDirection = FlexBox::Direction::column;
    }

    void initializeButtons()
    {
        /*
        // Mode when a track is selected (rename enabled)
        renameSelectionButtonActiveInfo = MultiButton::Mode {
            "RenameSelection-Active",
            [this] {}, // TODO
            Colours::darkblue,
            "Rename the currently selected track",
            MultiButton::DrawingMode::IconOnly,
            fontawesome::FileText,
        };
        // Mode when there is no track selected (rename disabled)
        renameSelectionButtonInactiveInfo = MultiButton::Mode {
            "RenameSelection-Inactive",
            [this] {},
            Colours::lightgrey,
            "No track selected",
            MultiButton::DrawingMode::IconOnly,
            fontawesome::FileText,
        };
        renameSelectionButton.addMode(renameSelectionButtonActiveInfo);
        renameSelectionButton.addMode(renameSelectionButtonInactiveInfo);
        buttonsComponent.addAndMakeVisible(renameSelectionButton);
        */

        addFileButtonInfo = MultiButton::Mode {
            "AddFile",
            [this] { addFileCallback(); },
            Colours::lightblue,
            "Click to add a file to the media clipboard",
            MultiButton::DrawingMode::IconOnly,
            fontawesome::Folder,
        };
        addFileButton.addMode(addFileButtonInfo);
        buttonsComponent.addAndMakeVisible(addFileButton);

        // Mode when a track is selected (remove enabled)
        removeSelectionButtonActiveInfo = MultiButton::Mode {
            "RemoveSelection-Active",
            [this] {}, // TODO
            Colours::orangered,
            "Remove the currently selected track from the media clipboard",
            MultiButton::DrawingMode::IconOnly,
            fontawesome::Remove,
        };
        // Mode when there is no track selected (rename disabled)
        removeSelectionButtonInactiveInfo = MultiButton::Mode {
            "RemoveSelection-Inactive",
            [this] {},
            Colours::lightgrey,
            "No track selected",
            MultiButton::DrawingMode::IconOnly,
            fontawesome::Remove,
        };
        removeSelectionButton.addMode(removeSelectionButtonActiveInfo);
        removeSelectionButton.addMode(removeSelectionButtonInactiveInfo);
        buttonsComponent.addAndMakeVisible(removeSelectionButton);

        // Mode when a playable track is selected (play enabled)
        playButtonActiveInfo = MultiButton::Mode {
            "Play-Active",
            [this] {}, // TODO
            Colours::limegreen,
            "Click to start playback",
            MultiButton::DrawingMode::IconOnly,
            fontaudio::Play,
        };
        // Mode when there is no track selected (play disabled)
        playButtonInactiveInfo = MultiButton::Mode {
            "Play-Inactive",
            [this] {},
            Colours::lightgrey,
            "Nothing to play",
            MultiButton::DrawingMode::IconOnly,
            fontaudio::Play,
        };
        // Mode during playback (stop enabled)
        stopButtonInfo = MultiButton::Mode {
            "Stop",
            [this] {}, // TODO
            Colours::orangered,
            "Click to stop playback",
            MultiButton::DrawingMode::IconOnly,
            fontaudio::Stop,
        };
        playStopButton.addMode(playButtonActiveInfo);
        playStopButton.addMode(playButtonInactiveInfo);
        playStopButton.addMode(stopButtonInfo);
        buttonsComponent.addAndMakeVisible(playStopButton);

        // Mode when a track is selected (save file enabled)
        saveFileButtonActiveInfo = MultiButton::Mode {
            "Save-Active",
            [this] { saveFileCallback(); },
            Colours::lightblue,
            "Click to save the media file",
            MultiButton::DrawingMode::IconOnly,
            fontawesome::Save,
        };
        // Mode when there is no track selected (save file disabled)
        saveFileButtonInactiveInfo = MultiButton::Mode {
            "Save-Inactive",
            [this] {},
            Colours::lightgrey,
            "Nothing to save",
            MultiButton::DrawingMode::IconOnly,
            fontawesome::Save,
        };
        saveFileButton.addMode(saveFileButtonActiveInfo);
        saveFileButton.addMode(saveFileButtonInactiveInfo);
        buttonsComponent.addAndMakeVisible(saveFileButton);
    }

    ~MediaClipboardWidget() { trackAreaWidget.removeChangeListener(this); }

    void paint(Graphics& g) { g.fillAll(Colours::lightgrey.darker().withAlpha(0.5f)); }

    void resized() override
    {
        Rectangle<int> totalBounds = getLocalBounds();

        // Remove existing items in main flex
        mainFlexBox.items.clear();

        // Add control bar and track area to flex
        mainFlexBox.items.add(
            FlexItem(controlsComponent)
                .withHeight(30)
                .withMargin(marginSize)); //jmax(30, trackNameLabel.getFont().getHeight()))
        mainFlexBox.items.add(FlexItem(trackAreaWidget)
                                  .withFlex(10)
                                  .withMargin({ 0, marginSize, marginSize, marginSize }));

        mainFlexBox.performLayout(totalBounds);

        // Remove existing items in main flex
        controlsFlexBox.items.clear();

        // Add control elements to control flex
        controlsFlexBox.items.add(FlexItem(selectionTextBox).withFlex(1));
        controlsFlexBox.items.add(
            FlexItem(buttonsComponent).withWidth(4 * (buttonWidth + marginSize)));

        controlsFlexBox.performLayout(controlsComponent.getLocalBounds());

        // Remove existing items in button flex
        buttonsFlexBox.items.clear();

        // Add buttons to flex with equal width
        /*buttonsFlexBox.items.add(FlexItem(renameSelectionButton)
                                     .withWidth(buttonWidth)
                                     .withHeight(buttonWidth)
                                     .withMargin({ 0, 0, 0, marginSize }));*/
        buttonsFlexBox.items.add(FlexItem(addFileButton)
                                     .withWidth(buttonWidth)
                                     .withHeight(buttonWidth)
                                     .withMargin({ 0, 0, 0, marginSize }));
        buttonsFlexBox.items.add(FlexItem(removeSelectionButton)
                                     .withWidth(buttonWidth)
                                     .withHeight(buttonWidth)
                                     .withMargin({ 0, 0, 0, marginSize }));
        buttonsFlexBox.items.add(FlexItem(playStopButton)
                                     .withWidth(buttonWidth)
                                     .withHeight(buttonWidth)
                                     .withMargin({ 0, 0, 0, marginSize }));
        buttonsFlexBox.items.add(FlexItem(saveFileButton)
                                     .withWidth(buttonWidth)
                                     .withHeight(buttonWidth)
                                     .withMargin({ 0, 0, 0, marginSize }));

        buttonsFlexBox.performLayout(buttonsComponent.getLocalBounds());
    }

    void resetControlState()
    {
        selectionTextBox.clear();
        selectionTextBox.setEnabled(false);

        //renameSelectionButton.setMode(renameSelectionButtonInactiveInfo.label);
        addFileButton.setMode(addFileButtonInfo.label);
        removeSelectionButton.setMode(removeSelectionButtonInactiveInfo.label);
        playStopButton.setMode(playButtonInactiveInfo.label);
        saveFileButton.setMode(saveFileButtonInactiveInfo.label);
    }

    void addFileCallback()
    {
        StringArray validExtensions = MediaDisplayComponent::getSupportedExtensions();
        String filePatternsAllowed = "*" + validExtensions.joinIntoString(";*");

        chooseFileBrowser =
            std::make_unique<FileChooser>("Select a media file...", File(), filePatternsAllowed);

        chooseFileBrowser->launchAsync(
            FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
            [this](const FileChooser& fc)
            {
                File chosenFile = fc.getResult();
                if (chosenFile != File {})
                {
                    trackAreaWidget.addTrackFromFilePath(URL(chosenFile));
                }
            });
    }

    void saveFileCallback() {}

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override
    {
        MediaDisplayComponent* mediaDisplay = trackAreaWidget.getCurrentlySelectedDisplay();

        if (mediaDisplay && mediaDisplay != currentlySelectedDisplay)
        {
            // TODO
        }
    }

    const int marginSize = 2;
    const int buttonWidth = 26;

    // Main controls component
    Component controlsComponent;
    // Text editor subcomponent for track label of current selection
    TextEditor selectionTextBox;
    // Buttons area subcomponent
    Component buttonsComponent;

    // Button components
    MultiButton renameSelectionButton;
    MultiButton::Mode renameSelectionButtonActiveInfo;
    MultiButton::Mode renameSelectionButtonInactiveInfo;

    MultiButton addFileButton;
    MultiButton::Mode addFileButtonInfo;

    MultiButton removeSelectionButton;
    MultiButton::Mode removeSelectionButtonActiveInfo;
    MultiButton::Mode removeSelectionButtonInactiveInfo;

    MultiButton playStopButton;
    MultiButton::Mode playButtonActiveInfo;
    MultiButton::Mode playButtonInactiveInfo;
    MultiButton::Mode stopButtonInfo;

    MultiButton saveFileButton;
    MultiButton::Mode saveFileButtonActiveInfo;
    MultiButton::Mode saveFileButtonInactiveInfo;

    //Viewport trackArea;
    //TrackAreaWidget* trackAreaWidget;
    TrackAreaWidget trackAreaWidget { DisplayMode::Thumbnail, 75 };

    // Flex for whole media clipboard
    FlexBox mainFlexBox;
    // Flex for controls area (text box and buttons)
    FlexBox controlsFlexBox;
    // Flex for buttons
    FlexBox buttonsFlexBox;

    std::unique_ptr<FileChooser> chooseFileBrowser;

    MediaDisplayComponent* currentlySelectedDisplay;
};
