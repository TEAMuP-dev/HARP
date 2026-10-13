/**
 * @file MediaClipboardWidget.h
 * @brief Component that manages cached (non-model-specific) media files HARP.
 * @author cwitkowitz, rzhu15
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "TrackAreaWidget.h"

#include "ModelStyle.h"
#include "PreviewPaneWidget.h"

#include "../gui/MultiButton.h"

#include "../utils/Interface.h"
#include "../utils/Logging.h"

#include "../windows/tutorial/TutorialTargets.h"

using namespace juce;

// Strip along the left edge of the media clipboard that is dragged to change its width
class ResizeEdgeComponent : public Component
{
public:
    ResizeEdgeComponent() { setMouseCursor(MouseCursor::LeftRightResizeCursor); }

    // Called once when a drag begins
    std::function<void()> onDragStart;
    // Called throughout a drag with how far right the mouse is from where the drag began
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
            onDrag(e.getScreenX() - e.getMouseDownScreenX());
        }
    }

    void mouseUp(const MouseEvent&) override { repaint(); }
    void mouseEnter(const MouseEvent&) override { repaint(); }
    void mouseExit(const MouseEvent&) override { repaint(); }

    // Divides the clipboard from the model tab, and lights up while hovered like a card outline
    void paint(Graphics& g) override
    {
        const bool isActive = isMouseOverOrDragging();

        g.setColour(isActive ? ModelStyle::accent.withAlpha(0.6f) : ModelStyle::outline);
        g.fillRect(getLocalBounds().removeFromLeft(isActive ? 2 : 1));
    }
};

class MediaClipboardWidget : public Component, public ChangeListener
{
public:
    MediaClipboardWidget(DragOverlayComponent* overlay)
        : dragOverlay(overlay), trackAreaWidget(DisplayMode::Thumbnail, thumbnailHeight, overlay)
    {
        initializeButtons();
        addAndMakeVisible(controlsComponent);
        controlsComponent.setComponentID(TutorialTargets::clipboardControls);

        resetState();

        trackAreaWidget.addChangeListener(this);
        trackAreaWidget.onTrackMediaDoubleClicked = [this]
        {
            setPreviewPaneVisible(true);
            previewPaneWidget.setMinimized(false);
        };
        trackArea.setViewedComponent(&trackAreaWidget, false);
        addAndMakeVisible(trackArea);
        previewPaneWidget.onResize = [this](int newHeight)
        {
            previewPaneHeight =
                jlimit(PreviewPaneWidget::minimumHeight, getMaximumPreviewPaneHeight(), newHeight);
            resized();
        };
        previewPaneWidget.onMinimizeChanged = [this] { resized(); };
        previewPaneWidget.onClose = [this] { setPreviewPaneVisible(false); };
        addAndMakeVisible(previewPaneWidget);

        resizeEdge.onDragStart = [this] { widthAtDragStart = getWidth(); };
        resizeEdge.onDrag = [this](int distanceRight)
        {
            // The clipboard sits at the right of the window, so dragging left widens it
            if (onResize)
            {
                onResize(jmax(minimumWidth, widthAtDragStart - distanceRight));
            }
        };
        addAndMakeVisible(resizeEdge);
    }

    ~MediaClipboardWidget() override { trackAreaWidget.removeChangeListener(this); }

    void paint(Graphics& g) override
    {
        // The background of the model tab beside it
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));

        // Headed like a section of a model tab, with the buttons at the end of its rule
        ModelStyle::drawSectionHeader(
            g,
            "Media Clipboard",
            controlsComponent.getBounds().withTrimmedRight(
                numButtons * static_cast<int>(buttonWidth + marginSize) + headerGap));
    }

    void resized() override
    {
        resizeEdge.setBounds(0, 0, resizeEdgeWidth, getHeight());

        // Space around the clipboard, as around a model tab
        Rectangle<int> totalBounds = getLocalBounds().reduced(pagePadding);

        // Flex for whole media clipboard
        FlexBox mainFlexBox;
        mainFlexBox.flexDirection = FlexBox::Direction::column;

        // Add header and track area to flex
        mainFlexBox.items.add(
            FlexItem(controlsComponent).withHeight((float) ModelStyle::sectionHeaderHeight));
        mainFlexBox.items.add(FlexItem(trackArea).withFlex(10));

        // Add preview pane to flex
        if (showPreviewPane)
        {
            const int paneHeight = previewPaneWidget.isMinimized()
                                       ? PreviewPaneWidget::titleBarHeight
                                       : jmin(previewPaneHeight, getMaximumPreviewPaneHeight());

            mainFlexBox.items.add(FlexItem(previewPaneWidget)
                                      .withHeight(static_cast<float>(paneHeight))
                                      .withMargin({ (float) sectionGap, 0, 0, 0 }));
        }
        else
        {
            previewPaneWidget.setBounds(0, 0, 0, 0);
        }

        mainFlexBox.performLayout(totalBounds);

        // Flex for buttons
        FlexBox buttonsFlexBox;
        buttonsFlexBox.flexDirection = FlexBox::Direction::row;
        buttonsFlexBox.alignItems = FlexBox::AlignItems::center;
        buttonsFlexBox.justifyContent = FlexBox::JustifyContent::flexEnd;

        // Add buttons to flex with equal width
        buttonsFlexBox.items.add(FlexItem(addFileButton)
                                     .withWidth(buttonWidth)
                                     .withHeight(buttonWidth)
                                     .withMargin({ 0, marginSize, 0, 0 }));
        buttonsFlexBox.items.add(FlexItem(removeSelectionButton)
                                     .withWidth(buttonWidth)
                                     .withHeight(buttonWidth)
                                     .withMargin({ 0, marginSize, 0, 0 }));
        buttonsFlexBox.items.add(FlexItem(saveFileButton)
                                     .withWidth(buttonWidth)
                                     .withHeight(buttonWidth)
                                     .withMargin({ 0, marginSize, 0, 0 }));
        buttonsFlexBox.items.add(FlexItem(sendToDAWButton)
                                     .withWidth(buttonWidth)
                                     .withHeight(buttonWidth)
                                     .withMargin({ 0, marginSize, 0, 0 }));

        buttonsFlexBox.performLayout(controlsComponent.getLocalBounds());

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

    std::function<void(int)> onResize;

    int getDefaultWidth() const { return defaultWidth; }
    int getMinimumWidth() const { return minimumWidth; }

    // Called whenever preview pane is shown or hidden
    std::function<void()> onPreviewPaneVisibilityChanged;

    bool isPreviewPaneVisible() const { return showPreviewPane; }

    void setPreviewPaneVisible(bool shouldBeVisible)
    {
        if (showPreviewPane == shouldBeVisible)
        {
            return;
        }

        showPreviewPane = shouldBeVisible;

        if (showPreviewPane)
        {
            previewPaneWidget.setMinimized(false);

            if (currentlySelectedDisplay != nullptr)
            {
                previewPaneWidget.showTrack(currentlySelectedDisplay->getOriginalFilePath());
            }
        }
        else
        {
            previewPaneWidget.clearTrack();
        }

        resized();

        if (onPreviewPaneVisibilityChanged)
        {
            onPreviewPaneVisibilityChanged();
        }
    }

    void stopPlayback() { previewPaneWidget.stopPlayback(); }

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

                                    StringArray validExtensions =
                                        originalTrack->getInstanceExtensions();

                                    if (! validExtensions.contains(selectedFile.getFileExtension()))
                                    {
                                        AlertWindow::showMessageBoxAsync(
                                            AlertWindow::WarningIcon,
                                            "Invalid File",
                                            "This track cannot be overwritten with the selected file.",
                                            "OK");

                                        return;
                                    }

                                    bool successfulOverwrite = false;

                                    String ext = originalFile.getFileExtension().toLowerCase();

                                    if (AudioDisplayComponent::getSupportedExtensions().contains(
                                            ext)
                                        && ext != selectedFile.getFileExtension().toLowerCase())
                                    {
                                        File tempFile = selectedFile.getSiblingFile(
                                            selectedFile.getFileNameWithoutExtension()
                                            + "_converted" + originalFile.getFileExtension());

                                        if (convertAudioFile(selectedFile, tempFile))
                                        {
                                            if (tempFile.copyFileTo(originalFile))
                                            {
                                                DBG_AND_LOG(
                                                    "MediaClipboardWidget::sendToDAWCallback: Converted and overwrote \""
                                                    << originalFile.getFullPathName()
                                                    << "\" with \""
                                                    << selectedFile.getFullPathName() << "\".");

                                                successfulOverwrite = true;
                                            }
                                            else
                                            {
                                                DBG_AND_LOG(
                                                    "MediaClipboardWidget::sendToDAWCallback: Conversion succeeded "
                                                    "but failed to copy to \""
                                                    << originalFile.getFullPathName() << "\".");
                                            }

                                            tempFile.deleteFile();
                                        }
                                        else
                                        {
                                            AlertWindow::showMessageBoxAsync(
                                                AlertWindow::WarningIcon,
                                                "File Type Mismatch",
                                                "Cannot convert file of type \""
                                                    + selectedFile.getFileExtension() + "\" to \""
                                                    + originalFile.getFileExtension() + "\".",
                                                "OK");
                                        }
                                    }
                                    else
                                    {
                                        if (selectedFile.copyFileTo(originalFile))
                                        {
                                            DBG_AND_LOG(
                                                "MediaClipboardWidget::sendToDAWCallback: Overwriting file \""
                                                << originalFile.getFullPathName() << "\" with \""
                                                << selectedFile.getFullPathName() << "\".");

                                            successfulOverwrite = true;
                                        }
                                        else
                                        {
                                            DBG_AND_LOG(
                                                "MediaClipboardWidget::sendToDAWCallback: Failed to overwrite file \""
                                                << originalFile.getFullPathName() << "\" with \""
                                                << selectedFile.getFullPathName() << "\".");
                                        }
                                    }

                                    if (successfulOverwrite)
                                    {
                                        // Update display with overwritten media
                                        linkedDisplays[selectedIndex]->initializeDisplay(
                                            URL(originalFile));

                                        // Remove selected track
                                        removeSelectionCallback();

                                        // Select overwritten track
                                        linkedDisplays[selectedIndex]->selectTrack();
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
        addFileButtonInfo = MultiButton::Mode { "AddFile",
                                                "Click to add a file to the media clipboard.",
                                                [this] { addFileCallback(); },
                                                MultiButton::DrawingMode::IconOnly,
                                                Colours::lightblue,
                                                fontawesome::Folder };
        addFileButton.addMode(addFileButtonInfo);
        controlsComponent.addAndMakeVisible(addFileButton);

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
        controlsComponent.addAndMakeVisible(removeSelectionButton);

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
        controlsComponent.addAndMakeVisible(saveFileButton);

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
        controlsComponent.addAndMakeVisible(sendToDAWButton);
    }

    void changeListenerCallback(ChangeBroadcaster* /*source*/) override
    {
        MediaDisplayComponent* mediaDisplay = trackAreaWidget.getCurrentlySelectedDisplay();

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

    // TODO - move to utils/?
    bool convertAudioFile(const File& source, const File& target)
    {
        AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<AudioFormatReader> reader(formatManager.createReaderFor(source));

        if (! reader)
        {
            DBG_AND_LOG("MediaClipboardWidget::convertAudioFile: Could not read source file \""
                        << source.getFullPathName() << "\".");

            return false;
        }

        String ext = target.getFileExtension().toLowerCase();
        AudioFormat* format = nullptr;

        if (ext == ".wav")
            format = formatManager.findFormatForFileExtension("wav");
        else if (ext == ".aiff" || ext == ".aif")
            format = formatManager.findFormatForFileExtension("aiff");
        else if (ext == ".flac")
            format = formatManager.findFormatForFileExtension("flac");
        else
        {
            DBG_AND_LOG("MediaClipboardWidget::convertAudioFile: Unsupported target format \""
                        << ext << "\".");

            return false;
        }

        target.deleteFile();

        auto outputStream = target.createOutputStream();

        if (! outputStream)
        {
            DBG_AND_LOG("MediaClipboardWidget::convertAudioFile: Could not create output file \""
                        << target.getFullPathName() << "\".");

            return false;
        }

        std::unique_ptr<AudioFormatWriter> writer(format->createWriterFor(
            outputStream.release(), reader->sampleRate, reader->numChannels, 16, {}, 0));

        if (! writer)
        {
            DBG_AND_LOG("convertAudioFile: Could not create writer for \""
                        << target.getFullPathName() << "\".");

            return false;
        }

        const int blockSize = 4096;
        AudioBuffer<float> buffer((int) reader->numChannels, blockSize);
        int64 totalFrames = (int64) reader->lengthInSamples;
        int64 position = 0;

        while (position < totalFrames)
        {
            int64 framesToRead = jmin((int64) blockSize, totalFrames - position);
            reader->read(&buffer, 0, (int) framesToRead, position, true, true);
            writer->writeFromAudioSampleBuffer(buffer, 0, (int) framesToRead);
            position += framesToRead;
        }

        DBG_AND_LOG("MediaClipboardWidget::convertAudioFile: Converted and saved \""
                    << source.getFullPathName() << "\" to \"" << target.getFullPathName() << "\".");

        return true;
    }

    void resetState()
    {
        addFileButton.setMode(addFileButtonInfo.displayLabel);
        removeSelectionButton.setMode(removeSelectionButtonInactiveInfo.displayLabel);
        saveFileButton.setMode(saveFileButtonInactiveInfo.displayLabel);
        sendToDAWButton.setMode(sendToDAWButtonInactiveInfo1.displayLabel);

        previewPaneWidget.clearTrack();

        currentlySelectedDisplay = nullptr;
    }

    void selectTrack(MediaDisplayComponent* mediaDisplay)
    {
        removeSelectionButton.setMode(removeSelectionButtonActiveInfo.displayLabel);
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

        if (showPreviewPane)
        {
            previewPaneWidget.showTrack(mediaDisplay->getOriginalFilePath());
        }

        currentlySelectedDisplay = mediaDisplay;
    }

    // The tallest the preview pane can be while leaving room for the header and a track
    int getMaximumPreviewPaneHeight() const
    {
        const int otherSpace =
            2 * pagePadding + ModelStyle::sectionHeaderHeight + sectionGap + minimumTrackListHeight;

        return jmax(PreviewPaneWidget::minimumHeight, getHeight() - otherSpace);
    }

    const float marginSize = 2;
    const float buttonWidth = 24;
    // Buttons at the end of the header, where the title and rule fill the space left of them
    static constexpr int numButtons = 4;
    static constexpr int headerGap = 8;
    // Space around the clipboard, and between its sections, as in a model tab
    static constexpr int pagePadding = 8;
    static constexpr int sectionGap = 8;
    static constexpr int thumbnailHeight = 75;
    // Room for one track in the list, with the margins the track area gives it
    static constexpr int minimumTrackListHeight = thumbnailHeight + 8;
    const int resizeEdgeWidth = 6;
    const int defaultWidth = 250;
    const int minimumWidth = 150;

    // Bar of buttons above the track list
    Component controlsComponent;
    // Draggable edge component
    ResizeEdgeComponent resizeEdge;
    int widthAtDragStart = defaultWidth;

    // Button components
    MultiButton addFileButton;
    MultiButton::Mode addFileButtonInfo;

    MultiButton removeSelectionButton;
    MultiButton::Mode removeSelectionButtonActiveInfo;
    MultiButton::Mode removeSelectionButtonInactiveInfo;

    MultiButton saveFileButton;
    MultiButton::Mode saveFileButtonActiveInfo;
    MultiButton::Mode saveFileButtonInactiveInfo;

    MultiButton sendToDAWButton;
    MultiButton::Mode sendToDAWButtonActiveInfo;
    MultiButton::Mode sendToDAWButtonSelectedInfo;
    MultiButton::Mode sendToDAWButtonInactiveInfo1;
    MultiButton::Mode sendToDAWButtonInactiveInfo2;

    DragOverlayComponent* dragOverlay = nullptr;

    Viewport trackArea;
    TrackAreaWidget trackAreaWidget { DisplayMode::Thumbnail, thumbnailHeight };

    PreviewPaneWidget previewPaneWidget;
    int previewPaneHeight = PreviewPaneWidget::defaultHeight;
    bool showPreviewPane = true;

    std::unique_ptr<FileChooser> chooseFileBrowser;

    MediaDisplayComponent* currentlySelectedDisplay = nullptr;
};
