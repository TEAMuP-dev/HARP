/**
 * @file MediaClipboardWidget.h
 * @brief Component that manages cached (non-model-specific) media files HARP.
 * @author cwitkowitz
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "TrackAreaWidget.h"

#include "../gui/MultiButton.h"

#include "../utils/Logging.h"

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

        trackAreaWidget.addChangeListener(this);
        trackArea.setViewedComponent(&trackAreaWidget, false);
        addAndMakeVisible(trackArea);
    }

    ~MediaClipboardWidget() { trackAreaWidget.removeChangeListener(this); }

    void paint(Graphics& g) { g.fillAll(Colours::lightgrey.darker().withAlpha(0.5f)); }

    void resized() override
    {
        Rectangle<int> totalBounds = getLocalBounds();

        // Flex for whole media clipboard
        FlexBox mainFlexBox;
        mainFlexBox.flexDirection = FlexBox::Direction::column;

        // Add control bar and track area to flex
        mainFlexBox.items.add(
            FlexItem(controlsComponent)
                .withHeight(30)
                .withMargin(marginSize)); //jmax(30, trackNameLabel.getFont().getHeight()))
        mainFlexBox.items.add(
            FlexItem(trackArea).withFlex(10).withMargin({ 0, marginSize, marginSize, marginSize }));

        mainFlexBox.performLayout(totalBounds);

        // Flex for controls area (text box and buttons)
        FlexBox controlsFlexBox;
        controlsFlexBox.flexDirection = FlexBox::Direction::row;
        controlsFlexBox.alignItems = FlexBox::AlignItems::stretch;

        // Add control elements to control flex
        controlsFlexBox.items.add(FlexItem(selectionTextBox).withFlex(1));
        controlsFlexBox.items.add(
            FlexItem(buttonsComponent).withWidth(5 * (buttonWidth + marginSize)));

        controlsFlexBox.performLayout(controlsComponent.getLocalBounds());

        // Flex for buttons
        FlexBox buttonsFlexBox;
        buttonsFlexBox.flexDirection = FlexBox::Direction::row;
        buttonsFlexBox.alignItems = FlexBox::AlignItems::center;
        buttonsFlexBox.justifyContent = FlexBox::JustifyContent::center;

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

        int visibleWidthBeforeResize = trackArea.getMaximumVisibleWidth();
        int visibleHeightBeforeResize = trackArea.getMaximumVisibleHeight();

        trackAreaWidget.setFixedTotalDimensions(visibleWidthBeforeResize,
                                                visibleHeightBeforeResize);

        int visibleWidthAfterResize = trackArea.getMaximumVisibleWidth();
        int visibleHeightAfterResize = trackArea.getMaximumVisibleHeight();

        if (visibleWidthBeforeResize != visibleWidthAfterResize
            || visibleHeightBeforeResize != visibleHeightAfterResize)
        {
            /*
              Correct track area dimensions after the initial resize. This is
              needed for situations where the track area widget is resized as a
              result of the media clipboard being resized, in order to properly
              register changes to viewport scroll bar visibility.
            */
            trackAreaWidget.setFixedTotalDimensions(visibleWidthAfterResize,
                                                    visibleHeightAfterResize);
        }
    }

    void addTrackFromFilePath(URL filePath, bool fromDAW = false)
    {
        // TODO - is there an explicit way to check how HARP was invoked?
        trackAreaWidget.addTrackFromFilePath(filePath, fromDAW);
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
                                   .withTitle("Send to DAW")
                                   .withMessage("Select a track to overwrite")
                                   .withIconType(MessageBoxIconType::NoIcon)
                                   .withButton("Overwrite")
                                   .withButton("Cancel");

                std::unique_ptr<AlertWindow> alertWindow = std::make_unique<AlertWindow>(
                    options.getTitle(), options.getMessage(), options.getIconType());

                alertWindow->addButton(options.getButtonText(0), 0); // Overwrite
                alertWindow->addButton(options.getButtonText(1), 1); // Cancel

                auto linkedTracksDropdown = std::make_unique<ComboBox>("DAW-Linked Tracks");
                linkedTracksDropdown->setSize(275, 24);
                linkedTracksDropdown->setName("");

                for (unsigned int i = 0; i < linkedDisplays.size(); i++)
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
                                unsigned int selectedIndex =
                                    linkedTracksDropdown->getSelectedId() - 1;

                                if (selectedIndex >= 0 && selectedIndex < linkedDisplays.size())
                                {
                                    MediaDisplayComponent* originalTrack =
                                        linkedDisplays[selectedIndex];

                                    File originalFile =
                                        originalTrack->getOriginalFilePath().getLocalFile();
                                    File selectedFile =
                                        selectedTrack->getOriginalFilePath().getLocalFile();

                                    /*StringArray validExtensions =
                                        originalTrack->getInstanceExtensions();

                                    if (! validExtensions.contains(selectedFile.getFileExtension()))
                                    {
                                        AlertWindow::showMessageBoxAsync(
                                            AlertWindow::WarningIcon,
                                            "Invalid File",
                                            "This track can only be overwritten with data of the following file types: "
                                                + validExtensions.joinIntoString(", ") + ".",
                                            "OK");
                                    }*/

                                    if (originalFile.getFileExtension()
                                        != selectedFile.getFileExtension())
                                    {
                                        AlertWindow::showMessageBoxAsync(
                                            AlertWindow::WarningIcon,
                                            "File Type Mismatch",
                                            "Cannot overwrite file of type \""
                                                + originalFile.getFileExtension()
                                                + "\" with file of type \""
                                                + selectedFile.getFileExtension() + "\".",
                                            "OK");

                                        // TODO - perform conversion if possible
                                    }
                                    else
                                    {
                                        if (selectedFile.copyFileTo(originalFile))
                                        {
                                            DBG_AND_LOG(
                                                "MediaClipboardWidget::sendToDAWCallback: Overwriting file "
                                                << originalFile.getFullPathName() << " with "
                                                << selectedFile.getFullPathName() << ".");

                                            // Update display with overwritten media
                                            linkedDisplays[selectedIndex]->initializeDisplay(
                                                URL(originalFile));

                                            // Remove selected track
                                            removeSelectionCallback();

                                            // Select overwritten track
                                            linkedDisplays[selectedIndex]->selectTrack();
                                        }
                                        else
                                        {
                                            DBG_AND_LOG(
                                                "MediaClipboardWidget::sendToDAWCallback: Failed to overwrite file "
                                                << originalFile.getFullPathName() << " with "
                                                << selectedFile.getFullPathName() << ".");
                                        }
                                    }
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
            "Rename the currently selected track.",
            [this] {}, // TODO
            MultiButton::DrawingMode::IconOnly,
            Colours::darkblue,
            fontawesome::FileText
        };
        // Mode when there is no track selected (rename disabled)
        renameSelectionButtonInactiveInfo = MultiButton::Mode {
            "RenameSelection-Inactive",
            "No track selected.",
            [this] {},
            MultiButton::DrawingMode::IconOnly,
            Colours::lightgrey,
            fontawesome::FileText
        };
        renameSelectionButton.addMode(renameSelectionButtonActiveInfo);
        renameSelectionButton.addMode(renameSelectionButtonInactiveInfo);
        buttonsComponent.addAndMakeVisible(renameSelectionButton);
        */

        addFileButtonInfo = MultiButton::Mode { "AddFile",
                                                "Click to add a file to the media clipboard.",
                                                [this] { addFileCallback(); },
                                                MultiButton::DrawingMode::IconOnly,
                                                Colours::lightblue,
                                                fontawesome::Folder };
        addFileButton.addMode(addFileButtonInfo);
        buttonsComponent.addAndMakeVisible(addFileButton);

        // Mode when a track is selected (remove enabled)
        removeSelectionButtonActiveInfo = MultiButton::Mode {
            "RemoveSelection-Active",
            "Click to remove the currently selected track from the media clipboard.",
            [this] { removeSelectionCallback(); },
            MultiButton::DrawingMode::IconOnly,
            Colours::orangered,
            fontawesome::Remove
        };
        // Mode when there is no track selected (rename disabled)
        removeSelectionButtonInactiveInfo = MultiButton::Mode {
            "RemoveSelection-Inactive",         "No track selected.", [this] {},
            MultiButton::DrawingMode::IconOnly, Colours::lightgrey,   fontawesome::Remove
        };
        removeSelectionButton.addMode(removeSelectionButtonActiveInfo);
        removeSelectionButton.addMode(removeSelectionButtonInactiveInfo);
        buttonsComponent.addAndMakeVisible(removeSelectionButton);

        // Mode when a playable track is selected (play enabled)
        playButtonActiveInfo = MultiButton::Mode { "Play-Active",
                                                   "Click to start playback.",
                                                   [this] { playCallback(); },
                                                   MultiButton::DrawingMode::IconOnly,
                                                   Colours::limegreen,
                                                   fontaudio::Play };
        // Mode when there is no track selected (play disabled)
        playButtonInactiveInfo =
            MultiButton::Mode { "Play-Inactive",    "Nothing to play.",
                                [this] {},          MultiButton::DrawingMode::IconOnly,
                                Colours::lightgrey, fontaudio::Play };
        // Mode during playback (stop enabled)
        stopButtonInfo = MultiButton::Mode { "Stop",
                                             "Click to stop playback.",
                                             [this] { stopCallback(); },
                                             MultiButton::DrawingMode::IconOnly,
                                             Colours::orangered,
                                             fontaudio::Stop };
        playStopButton.addMode(playButtonActiveInfo);
        playStopButton.addMode(playButtonInactiveInfo);
        playStopButton.addMode(stopButtonInfo);
        buttonsComponent.addAndMakeVisible(playStopButton);

        // Mode when a track is selected (save file enabled)
        saveFileButtonActiveInfo =
            MultiButton::Mode { "Save-Active",
                                "Click to save the currently selected media file.",
                                [this] { saveFileCallback(); },
                                MultiButton::DrawingMode::IconOnly,
                                Colours::lightblue,
                                fontawesome::Save };
        // Mode when there is no track selected (save file disabled)
        saveFileButtonInactiveInfo =
            MultiButton::Mode { "Save-Inactive",    "Nothing to save.",
                                [this] {},          MultiButton::DrawingMode::IconOnly,
                                Colours::lightgrey, fontawesome::Save };
        saveFileButton.addMode(saveFileButtonActiveInfo);
        saveFileButton.addMode(saveFileButtonInactiveInfo);
        buttonsComponent.addAndMakeVisible(saveFileButton);

        // Mode when DAW-linked tracks are available
        sendToDAWButtonActiveInfo = MultiButton::Mode {
            "SendToDAW-Active",
            "Click to overwrite an existing DAW-linked file with the selected media file.",
            [this] { sendToDAWCallback(); },
            MultiButton::DrawingMode::IconOnly,
            Colours::orange,
            fontawesome::ArrowCircleORight
        };
        // Mode when the only DAW-linked track is selected
        sendToDAWButtonSelectedInfo =
            MultiButton::Mode { "SendToDAW-Selected",
                                "This track is already DAW-linked. Select another one.",
                                [this] {},
                                MultiButton::DrawingMode::IconOnly,
                                Colours::lightgrey,
                                fontawesome::ArrowCircleORight };
        // Mode no track is selected
        sendToDAWButtonInactiveInfo1 = MultiButton::Mode { "SendToDAW-Inactive1",
                                                           "No track selected.",
                                                           [this] {},
                                                           MultiButton::DrawingMode::IconOnly,
                                                           Colours::lightgrey,
                                                           fontawesome::ArrowCircleORight };
        // Mode when DAW-linked tracks are unavailable
        sendToDAWButtonInactiveInfo2 =
            MultiButton::Mode { "SendToDAW-Inactive2",
                                "No DAW-linked files in media clipboard.",
                                [this] {},
                                MultiButton::DrawingMode::IconOnly,
                                Colours::lightgrey,
                                fontawesome::ArrowCircleORight };
        sendToDAWButton.addMode(sendToDAWButtonActiveInfo);
        sendToDAWButton.addMode(sendToDAWButtonSelectedInfo);
        sendToDAWButton.addMode(sendToDAWButtonInactiveInfo1);
        sendToDAWButton.addMode(sendToDAWButtonInactiveInfo2);
        buttonsComponent.addAndMakeVisible(sendToDAWButton);
    }

    void changeListenerCallback(ChangeBroadcaster* /*source*/) override
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
                // Reset play/stop button state for select and stop events (avoid infinite messages)
                playStopButton.setMode(playButtonActiveInfo.displayLabel);
            }
        }

        if (mediaDisplay)
        {
            if (mediaDisplay != currentlySelectedDisplay)
            {
                // Handle select events if selected display changes
                selectTrack(mediaDisplay);
                // Handle track area resizing after adding a track
                resized(); // TODO - decouple from track selection?
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

            // Handle track area resizing after removing a track
            resized();
        }
    }

    void playCallback()
    {
        MediaDisplayComponent* mediaDisplay = trackAreaWidget.getCurrentlySelectedDisplay();

        if (mediaDisplay)
        {
            mediaDisplay->start();

            playStopButton.setMode(stopButtonInfo.displayLabel);
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

            playStopButton.setMode(playButtonActiveInfo.displayLabel);
        }
    }

    void resetState()
    {
        selectionTextBox.clear();
        selectionTextBox.setEnabled(false);

        //renameSelectionButton.setMode(renameSelectionButtonInactiveInfo.label);
        addFileButton.setMode(addFileButtonInfo.displayLabel);
        removeSelectionButton.setMode(removeSelectionButtonInactiveInfo.displayLabel);
        playStopButton.setMode(playButtonInactiveInfo.displayLabel);
        saveFileButton.setMode(saveFileButtonInactiveInfo.displayLabel);
        sendToDAWButton.setMode(sendToDAWButtonInactiveInfo1.displayLabel);

        currentlySelectedDisplay = nullptr;
    }

    void selectTrack(MediaDisplayComponent* mediaDisplay)
    {
        selectionTextBox.setText(mediaDisplay->getTrackName());
        selectionTextBox.setEnabled(true);

        //renameSelectionButton.setMode(renameSelectionButtonActiveInfo.label);
        removeSelectionButton.setMode(removeSelectionButtonActiveInfo.displayLabel);
        playStopButton.setMode(playButtonActiveInfo.displayLabel);
        saveFileButton.setMode(saveFileButtonActiveInfo.displayLabel);

        int nOtherDAWLinkedTracks =
            trackAreaWidget.getDAWLinkedDisplays().size() - mediaDisplay->isLinkedToDAW();

        if (nOtherDAWLinkedTracks > 0)
        {
            sendToDAWButton.setMode(sendToDAWButtonActiveInfo.displayLabel);
        }
        else if (mediaDisplay->isLinkedToDAW())
        {
            sendToDAWButton.setMode(sendToDAWButtonSelectedInfo.displayLabel);
        }
        else
        {
            sendToDAWButton.setMode(sendToDAWButtonInactiveInfo2.displayLabel);
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
    MultiButton::Mode sendToDAWButtonSelectedInfo;
    MultiButton::Mode sendToDAWButtonInactiveInfo1;
    MultiButton::Mode sendToDAWButtonInactiveInfo2;

    Viewport trackArea;
    TrackAreaWidget trackAreaWidget { DisplayMode::Thumbnail, 75 };

    std::unique_ptr<FileChooser> chooseFileBrowser;

    MediaDisplayComponent* currentlySelectedDisplay;
};
