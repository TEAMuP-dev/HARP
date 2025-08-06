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
        selectionTextBox.onReturnKey = [this] { renameSelectionCallback(); };
        controlsComponent.addAndMakeVisible(selectionTextBox);
        initializeButtons();
        controlsComponent.addAndMakeVisible(buttonsComponent);
        addAndMakeVisible(controlsComponent);

        resetState();

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
            FlexItem(buttonsComponent).withWidth(5 * (buttonWidth + marginSize)));

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
        buttonsFlexBox.items.add(FlexItem(sendToDAWButton)
                                     .withWidth(buttonWidth)
                                     .withHeight(buttonWidth)
                                     .withMargin({ 0, 0, 0, marginSize }));

        buttonsFlexBox.performLayout(buttonsComponent.getLocalBounds());
    }

    void addExternalTrackFromFilePath(URL filePath)
    {
        // TODO - is there an explicit way to check how HARP was invoked?
        trackAreaWidget.addTrackFromFilePath(filePath, true);
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
                    trackAreaWidget.addTrackFromFilePath(URL(chosenFile), false);
                }
            });
    }

    void saveFileCallback()
    {
        MediaDisplayComponent* mediaDisplay = trackAreaWidget.getCurrentlySelectedDisplay();

        if (mediaDisplay)
        {
            mediaDisplay->saveFileCallback();
        }
    }

    void sendToDAWCallback()
    {
        MediaDisplayComponent* selectedTrack = trackAreaWidget.getCurrentlySelectedDisplay();
        std::vector<MediaDisplayComponent*> linkedDisplays = trackAreaWidget.getDAWLinkedDisplays();

        MessageManager::callAsync(
            [this, selectedTrack, linkedDisplays]()
            {
                // Show dialog with dropdown of available tracks
                auto options = MessageBoxOptions()
                                   .withTitle("Send To DAW")
                                   .withMessage("Select a track to overwrite")
                                   .withIconType(MessageBoxIconType::NoIcon)
                                   .withButton("Overwrite")
                                   .withButton("Cancel");

                std::unique_ptr<AlertWindow> alertWindow = std::make_unique<AlertWindow>(
                    options.getTitle(), options.getMessage(), options.getIconType());

                alertWindow->addButton(options.getButtonText(0), 0); // Overwrite
                alertWindow->addButton(options.getButtonText(1), 0); // Cancel

                auto linkedTracksDropdown = std::make_unique<ComboBox>("DAW-Linked Tracks");
                linkedTracksDropdown->setSize(275, 24);
                linkedTracksDropdown->setName("");

                for (uint i = 0; i < linkedDisplays.size(); i++)
                {
                    if (linkedDisplays[i] != selectedTrack)
                    {
                        linkedTracksDropdown->addItem(linkedDisplays[i]->getTrackName(), i + 1);
                    }
                }

                if (linkedDisplays.size() > 0)
                {
                    linkedTracksDropdown->setSelectedItemIndex(0);
                }

                alertWindow->addCustomComponent(linkedTracksDropdown.get());

                // Open window asynchronously
                alertWindow->enterModalState(
                    true,
                    ModalCallbackFunction::create(
                        [this,
                         alertWindow = alertWindow.release(),
                         selectedTrack,
                         linkedDisplays,
                         linkedTracksDropdown = std::move(linkedTracksDropdown)](int result)
                        {
                            if (result == 0) // Overwrite
                            {
                                uint selectedIndex = linkedTracksDropdown->getSelectedId() - 1;

                                if (selectedIndex >= 0 && selectedIndex < linkedDisplays.size())
                                {
                                    MediaDisplayComponent* originalTrack =
                                        linkedDisplays[selectedIndex];

                                    DBG("MediaClipboardWidget::sendToDAWCallback: Overwriting file "
                                        << originalTrack->getOriginalFilePath()
                                               .getLocalFile()
                                               .getFullPathName()
                                        << " with "
                                        << selectedTrack->getOriginalFilePath()
                                               .getLocalFile()
                                               .getFullPathName()
                                        << ".");

                                    // TODO - overwrite selected track

                                    // Remove currently selected track
                                    //removeSelectionCallback();

                                    // TODO - select overwritten track (2 calls)
                                }
                            }
                        }),
                    true);
            });
    }

private:
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
            [this] { removeSelectionCallback(); },
            Colours::orangered,
            "Click to remove the currently selected track from the media clipboard",
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
            [this] { playCallback(); },
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
            [this] { stopCallback(); },
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
            "Click to save the currently selected media file",
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

        // Mode when DAW-linked tracks are available
        sendToDAWButtonActiveInfo = MultiButton::Mode {
            "SendToDAW-Active",
            [this] { sendToDAWCallback(); },
            Colours::orange,
            "Click to overwrite an existing DAW-linked file with the selected media file",
            MultiButton::DrawingMode::IconOnly,
            fontawesome::ArrowCircleORight,
        };
        // Mode when DAW-linked tracks are unavailable
        sendToDAWButtonInactiveInfo = MultiButton::Mode {
            "SendToDAW-Inactive",
            [this] {},
            Colours::lightgrey,
            "No DAW-linked files in media clipboard",
            MultiButton::DrawingMode::IconOnly,
            fontawesome::ArrowCircleORight,
        };
        sendToDAWButton.addMode(sendToDAWButtonActiveInfo);
        sendToDAWButton.addMode(sendToDAWButtonInactiveInfo);
        buttonsComponent.addAndMakeVisible(sendToDAWButton);
    }

    void changeListenerCallback(ChangeBroadcaster* source) override
    {
        MediaDisplayComponent* mediaDisplay = trackAreaWidget.getCurrentlySelectedDisplay();

        if (currentlySelectedDisplay)
        {
            if (currentlySelectedDisplay->isPlaying())
            {
                // Cancel playback and reset play/stop button state for select and stop events
                stopCallback(currentlySelectedDisplay);
            }
            else
            {
                // Just reset play/stop button state for select and stop events (avoid infinite messages)
                playStopButton.setMode(playButtonActiveInfo.label);
            }
        }

        if (mediaDisplay)
        {
            if (mediaDisplay != currentlySelectedDisplay)
            {
                // Handle select events if selected display changes
                selectTrack(mediaDisplay);
            }
        }
        else
        {
            // Handle deselect events
            resetState();
        }
    }

    void renameSelectionCallback()
    {
        MediaDisplayComponent* mediaDisplay = trackAreaWidget.getCurrentlySelectedDisplay();

        String currentText = selectionTextBox.getText();

        if (mediaDisplay && ! currentText.isEmpty())
        {
            mediaDisplay->setTrackName(currentText);
        }
    }

    void removeSelectionCallback()
    {
        MediaDisplayComponent* mediaDisplay = trackAreaWidget.getCurrentlySelectedDisplay();

        if (mediaDisplay)
        {
            trackAreaWidget.removeTrack(mediaDisplay);
            resetState();
        }
    }

    void playCallback()
    {
        MediaDisplayComponent* mediaDisplay = trackAreaWidget.getCurrentlySelectedDisplay();

        if (mediaDisplay)
        {
            mediaDisplay->start();

            playStopButton.setMode(stopButtonInfo.label);
        }
    }

    void stopCallback(MediaDisplayComponent* mediaDisplay = nullptr)
    {
        if (! mediaDisplay)
        {
            mediaDisplay = trackAreaWidget.getCurrentlySelectedDisplay();
        }

        if (mediaDisplay)
        {
            mediaDisplay->stop();

            playStopButton.setMode(playButtonActiveInfo.label);
        }
    }

    void resetState()
    {
        selectionTextBox.clear();
        selectionTextBox.setEnabled(false);

        //renameSelectionButton.setMode(renameSelectionButtonInactiveInfo.label);
        addFileButton.setMode(addFileButtonInfo.label);
        removeSelectionButton.setMode(removeSelectionButtonInactiveInfo.label);
        playStopButton.setMode(playButtonInactiveInfo.label);
        saveFileButton.setMode(saveFileButtonInactiveInfo.label);
        sendToDAWButton.setMode(sendToDAWButtonInactiveInfo.label);

        currentlySelectedDisplay = nullptr;
    }

    void selectTrack(MediaDisplayComponent* mediaDisplay)
    {
        selectionTextBox.setText(mediaDisplay->getTrackName());
        selectionTextBox.setEnabled(true);

        //renameSelectionButton.setMode(renameSelectionButtonActiveInfo.label);
        removeSelectionButton.setMode(removeSelectionButtonActiveInfo.label);
        playStopButton.setMode(playButtonActiveInfo.label);
        saveFileButton.setMode(saveFileButtonActiveInfo.label);

        int nOtherDAWLinkedTracks =
            trackAreaWidget.getDAWLinkedDisplays().size() - mediaDisplay->isLinkedToDAW();

        if (nOtherDAWLinkedTracks > 0)
        {
            sendToDAWButton.setMode(sendToDAWButtonActiveInfo.label);
        }
        else
        {
            sendToDAWButton.setMode(sendToDAWButtonInactiveInfo.label);
        }

        currentlySelectedDisplay = mediaDisplay;
    }

    const float marginSize = 2;
    const float buttonWidth = 26;

    // Main controls component
    Component controlsComponent;
    // Text editor subcomponent for track label of current selection
    TextEditor selectionTextBox;
    // Buttons area subcomponent
    Component buttonsComponent;

    // Button components
    /*MultiButton renameSelectionButton;
    MultiButton::Mode renameSelectionButtonActiveInfo;
    MultiButton::Mode renameSelectionButtonInactiveInfo;*/

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

    MultiButton sendToDAWButton;
    MultiButton::Mode sendToDAWButtonActiveInfo;
    MultiButton::Mode sendToDAWButtonInactiveInfo;

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
