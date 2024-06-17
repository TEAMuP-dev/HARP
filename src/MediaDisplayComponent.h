#pragma once

#include "juce_core/juce_core.h"

using namespace juce;


class MediaDisplayComponent : public Component,
                              public ChangeListener,
                              public ChangeBroadcaster,
                              public FileDragAndDropTarget,
                              private Timer,
                              private ScrollBar::Listener
{
public:

    enum ActionType {
        FileDropped,
        TransportMoved,
        TransportStarted
    };

    MediaDisplayComponent()
    {
        setupDisplay();

        currentPositionMarker.setFill(Colours::white.withAlpha(0.85f));
        addAndMakeVisible(currentPositionMarker);

        verticalZoomLabel.setEditable(false, false, false);
        verticalZoomLabel.setFont(Font(15.00f, Font::plain));
        verticalZoomLabel.setJustificationType(Justification::centredRight);
        verticalZoomLabel.setColour(TextEditor::textColourId, Colours::black);
        verticalZoomLabel.setColour(TextEditor::backgroundColourId, Colour (0x00000000));
        addAndMakeVisible(verticalZoomLabel);

        horizontalZoomLabel.setEditable(false, false, false);
        horizontalZoomLabel.setFont(Font(15.00f, Font::plain));
        horizontalZoomLabel.setJustificationType(Justification::centredRight);
        horizontalZoomLabel.setColour(TextEditor::textColourId, Colours::black);
        horizontalZoomLabel.setColour(TextEditor::backgroundColourId, Colour (0x00000000));
        addAndMakeVisible(horizontalZoomLabel);

        verticalZoomSlider.setSkewFactor(2);
        verticalZoomSlider.setRange(0, 1, 0);
        verticalZoomSlider.onValueChange = [this] { 
            this->setZoomFactor(verticalZoomSlider.getValue(), 1.0f); // TODO
        };
        addAndMakeVisible(verticalZoomSlider);

        horizontalZoomSlider.setSkewFactor(2);
        horizontalZoomSlider.setRange(0, 1, 0);
        horizontalZoomSlider.onValueChange = [this] { 
            this->setZoomFactor(horizontalZoomSlider.getValue(), 1.0f); // TODO
        };
        addAndMakeVisible(horizontalZoomSlider);

        playPauseButton.setColour (TextButton::buttonColourId, Colour (0xff79ed7f));
        playPauseButton.setColour (TextButton::textColourOffId, Colours::black);
        playPauseButton.onClick = [this] { togglePlay(); };
        addAndMakeVisible(playPauseButton);

        // TODO - reset (stop) button

        loadFileButton.onClick = [this] {
            loadFileBrowser();
        };
        addAndMakeVisible(loadFileButton);

        saveButton.onClick = [this] {
            File targetFile = targetFilePath.getLocalFile();
            File tempFile = tempFilePath.getLocalFile();

            String parentDirectory = targetFile.getParentDirectory().getFullPathName();
            String targetFileName = targetFile.getFileNameWithoutExtension();
            String targetFileExtension = targetFile.getFileExtension();

            File backupFile = File(parentDirectory + "/" + targetFileName + "_BACKUP" + targetFileExtension);

            DBG("MediaDisplayComponent.saveButton::onClick: Creating backup of file" << targetFile.getFullPathName() << " at "  << backupFile.getFullPathName() << ".");
            targetFile.copyFileTo(backupFile);

            DBG("MediaDisplayComponent.saveButton::onClick: Overwriting file " << targetFile.getFullPathName() << " with " << tempFile.getFullPathName() << ".");
            tempFile.moveFileTo(targetFile);

            generateTempFile();

            enableSaving(false);
        };
        addAndMakeVisible(saveButton);
    }

    void setupDisplay();

    void setupHandler()
    {
        mediaHandler = std::make_unique<HoverHandler>(this);
        mediaHandler->onMouseEnter = [this]() { 
            setInstructions(mediaHandlerInstructions); 
        };
        mediaHandler->onMouseExit = [this]() { clearInstructions(); };
        mediaHandler->attach();
    }

    void setZoomFactor(float xScale, float yScale);

    void loadFileBrowser()
    {
        fileBrowser = std::make_unique<FileChooser>("Select audio or midi file...", File(), "*.wav;*.aiff;*.mp3;*.flac;*.mid");
        fileBrowser->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                [this](const FileChooser& browser)
                                {
                                    File chosenFile = browser.getResult();
                                    if (chosenFile != File{})
                                    {
                                        targetFilePath = URL(chosenFile);
                                        setTargetFilePath(targetFilePath);
                                        loadMediaFile(targetFilePath);
                                        generateTempFile();
                                        resized();
                                    }
                                });
    }

    void setTargetFilePath(URL filePath)
    {
        targetFilePath = filePath;
    }

    void loadMediaFile(const URL& filePath);

    void generateTempFile() 
    {
        String docsDirectory = File::getSpecialLocation(File::SpecialLocationType::userDocumentsDirectory).getFullPathName();

        File targetFile = targetFilePath.getLocalFile();

        String targetFileName = targetFile.getFileNameWithoutExtension();
        String targetFileExtension = targetFile.getFileExtension();

        tempFilePath = URL(File(docsDirectory + "/HARP/" + targetFileName + "_harp" + targetFileExtension));

        File tempFile = tempFilePath.getLocalFile();

        tempFile.getParentDirectory().createDirectory();

        if (!targetFile.copyFileTo(tempFile)) {
            DBG("MediaDisplayComponent::createTempFile: Failed to copy file " << targetFile.getFullPathName() << " to " << tempFile.getFullPathName() << ".");
    
            AlertWindow("Error", "Failed to create temporary file for processing.", AlertWindow::WarningIcon);
        } else {
            DBG("MediaDisplayComponent::createTempFile: Copied file " << targetFile.getFullPathName() << " to " << tempFile.getFullPathName() << ".");
        }
    }

    void togglePlay();

    void resetPlay(); // TODO

    bool isPlaying();

    double getCurrentPosition();

    void enableSaving(bool enable)
    {
        saveButton.setEnabled(enable);
    }

    URL getTempFilePath()
    {
        return tempFilePath;
    }

    //void saveFileBrowser()
    //{
    //    fileBrowser = std::make_unique<FileChooser>("Save file...", File(), "*.wav;*.aiff;*.mp3;*.flac;*.mid");
        // TODO
    //}

    void resized() override
    {
        auto mainArea = getLocalBounds();

        //auto mediaRow = mainArea.removeFromTop();

        // TODO - media

        //verticalZoomLabel.setBounds();
        //verticalZoomSlider.setBounds();

        //auto controlRow1 = mainArea.removeFromTop();

        //horizontalZoomLabel.setBounds();
        //horizontalZoomSlider.setBounds();

        //followCursorButton.setBounds();

        //auto controlRow2 = mainArea.removeFromTop();

        //playPauseButton.setBounds();

        // TODO - reset (stop) button

        //loadFileButton.setBounds();

        //saveButton.setBounds();

    }

    void filesDropped (const StringArray& files, int /*x*/, int /*y*/) override
    {
        URL firstFilePath = URL(File(files[0]));

        //lastActionType = FileDropped;
        //sendChangeMessage();
        setTargetFilePath(firstFilePath);
        loadMediaFile(firstFilePath);
        generateTempFile();
    }

    //URL getLastDroppedFile() const noexcept { return lastFileDropped; }

    //ActionType getLastActionType() const noexcept { return lastActionType; }


private:
    URL targetFilePath;
    URL tempFilePath;

    DrawableRectangle currentPositionMarker;

    Label verticalZoomLabel{{}, "Vertical Scale"};
    Label horizontalZoomLabel{{}, "Horizontal Scale"};

    Slider verticalZoomSlider{Slider::LinearVertical, Slider::NoTextBox};
    Slider horizontalZoomSlider{Slider::LinearHorizontal, Slider::NoTextBox};

    TextButton playPauseButton{"Play"};

    // TODO - reset (stop) button

    TextButton loadFileButton{"Load File"};
    std::unique_ptr<FileChooser> fileBrowser;

    TextButton saveButton{"Save As"};
    //std::unique_ptr<FileChooser> fileBrowser;

    URL lastFileDropped;
    ActionType lastActionType;

    std::unique_ptr<HoverHandler> mediaHandler;

    float timeToX (const double time) const
    {
        if (visibleRange.getLength() <= 0)
            return 0;

        return (float) getWidth() * (float) ((time - visibleRange.getStart()) / visibleRange.getLength());
    }

    double xToTime (const float x) const
    {
        return (x / (float) getWidth()) * (visibleRange.getLength()) + visibleRange.getStart();
    }

    bool canMoveTransport() const noexcept
    {
        return !(isPlaying());
    }

    void scrollBarMoved (ScrollBar* scrollBarThatHasMoved, double newRangeStart) override
    {
        if (scrollBarThatHasMoved == &scrollbar)
            if (!(isPlaying()))
                setRange (visibleRange.movedToStartAt (newRangeStart));
    }

    void timerCallback() override
    {
        if (canMoveTransport())
            updateCursorPosition();
        else
            setRange (visibleRange.movedToStartAt (getCurrentPosition() - (visibleRange.getLength() / 2.0)));
    }

    void updateCursorPosition()
    {
        currentPositionMarker.setVisible(isPlaying() || isMouseButtonDown());
        currentPositionMarker.setRectangle(Rectangle<float>(timeToX (getCurrentPosition()) - 0.75f, 0,
                                                              1.5f, (float) (getHeight() - scrollbar.getHeight())));
    }
};
