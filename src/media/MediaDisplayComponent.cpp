#include "MediaDisplayComponent.h"
#include "AudioDisplayComponent.h"
#include "MidiDisplayComponent.h"

#include <cmath>

namespace
{
struct TickScheme
{
    double majorStep;
    double minorStep;
    int minorCount;
};

TickScheme chooseTickScheme(double visibleLength)
{
    static const TickScheme schemes[] = {
        { 0.1,   0.02,  5 },
        { 0.5,   0.1,   5 },
        { 1.0,   0.25,  4 },
        { 2.0,   0.5,   4 },
        { 5.0,   1.0,   5 },
        { 15.0,  5.0,   3 },
        { 30.0,  10.0,  3 },
        { 60.0,  15.0,  4 },
        { 120.0, 30.0,  4 },
        { 300.0, 60.0,  5 },
        { 600.0, 120.0, 5 },
    };

    for (const auto& s : schemes)
    {
        double numMajor = visibleLength / s.majorStep;
        if (numMajor >= 2.0 && numMajor <= 15.0)
            return s;
    }

    double majorStep = std::pow(10.0, std::floor(std::log10(visibleLength / 5.0)));
    majorStep = std::max(0.01, majorStep);
    return { majorStep, majorStep / 5.0, 5 };
}

String formatTime(double t, double step)
{
    if (t >= 3600.0)
    {
        int hrs = static_cast<int>(t / 3600.0);
        int mins = static_cast<int>(std::fmod(t, 3600.0) / 60.0);
        int secs = static_cast<int>(std::fmod(t, 60.0));
        return String(hrs) + "h " + String(mins) + "m " + String(secs) + "s";
    }
    if (t >= 60.0)
    {
        int mins = static_cast<int>(t / 60.0);
        int secs = static_cast<int>(std::fmod(t, 60.0));
        return String(mins) + "m " + String(secs) + "s";
    }
    if (step >= 1.0)
        return String(static_cast<int>(t)) + "s";

    return String(t, 2) + "s";
}
} // namespace

void TimeAxisStrip::paint(Graphics& g)
{
    if (owner == nullptr || ! owner->isFileLoaded())
        return;

    const auto& visibleRange = owner->getVisibleRange();
    const float pps = owner->getPixelsPerSecond();
    const double totalLength = owner->getTotalLengthInSecs();

    if (pps <= 0.0f || visibleRange.getLength() <= 0.0)
        return;

    const double visibleStart = visibleRange.getStart();
    const double visibleEnd = visibleRange.getEnd();
    const int w = getWidth();
    const int h = getHeight();

    g.setColour(Colours::darkgrey);
    g.fillRect(getLocalBounds());

    const double visibleLength = visibleRange.getLength();
    const auto scheme = chooseTickScheme(visibleLength);
    const double majorStep = scheme.majorStep;
    const double minorStep = scheme.minorStep;

    const float majorTickTop = 0.0f;
    const float majorTickBot = static_cast<float>(h);
    const float minorTickTop = static_cast<float>(h) * 0.55f;
    const float minorTickBot = static_cast<float>(h);

    // Minor ticks
    g.setColour(Colours::grey.withAlpha(0.5f));

    const double firstMinor = std::ceil(visibleStart / minorStep) * minorStep;
    for (double t = firstMinor; t <= visibleEnd && t <= totalLength; t += minorStep)
    {
        double remainder = std::fmod(t, majorStep);
        if (remainder < minorStep * 0.1 || (majorStep - remainder) < minorStep * 0.1)
            continue;

        const float x = static_cast<float>((t - visibleStart) * pps);
        if (x < 0.0f || x > static_cast<float>(w))
            continue;

        g.drawVerticalLine(static_cast<int>(x), minorTickTop, minorTickBot);
    }

    // Major ticks and labels
    g.setColour(Colours::lightgrey.withAlpha(0.9f));

    const int labelH = jmin(13, h - 2);
    g.setFont(static_cast<float>(labelH));

    const double firstMajor = std::ceil(visibleStart / majorStep) * majorStep;
    for (double t = firstMajor; t <= visibleEnd && t <= totalLength; t += majorStep)
    {
        const float x = static_cast<float>((t - visibleStart) * pps);
        if (x < -60.0f || x > static_cast<float>(w) + 60.0f)
            continue;

        g.drawVerticalLine(static_cast<int>(x), majorTickTop, majorTickBot);

        String label = formatTime(t, majorStep);
        g.drawText(label,
                   static_cast<int>(x) + 3,
                   0,
                   jmin(90, w - static_cast<int>(x)),
                   h,
                   Justification::centredLeft,
                   true);
    }
}

MediaDisplayComponent::MediaDisplayComponent() : MediaDisplayComponent("Media Track") {}

MediaDisplayComponent::MediaDisplayComponent(String name, bool req, bool fromDAW, DisplayMode mode)
    : trackName(name), required(req), linkedToDAW(fromDAW), displayMode(mode)
{
    formatManager.registerBasicFormats();

    deviceManager.initialise(0, 2, nullptr, true, {}, nullptr);
    deviceManager.addAudioCallback(&sourcePlayer);

    sourcePlayer.setSource(&transportSource);

    if (isLinkedToDAW())
    {
        headerComponent.setColor(linkedToDAWColor);
    }

    trackNameLabel.setText(trackName, dontSendNotification);
    trackNameLabel.setJustificationType(Justification::centred);
    headerComponent.addAndMakeVisible(trackNameLabel);
    headerComponent.addMouseListener(this, true);
    initializeButtons();
    addAndMakeVisible(headerComponent);

    headerFlexBox.flexDirection = FlexBox::Direction::row;
    headerFlexBox.alignItems = FlexBox::AlignItems::stretch;

    buttonsFlexBox.flexDirection = FlexBox::Direction::column;
    buttonsFlexBox.alignItems = FlexBox::AlignItems::center;
    buttonsFlexBox.justifyContent = FlexBox::JustifyContent::center;

    horizontalScrollBar.setAutoHide(false);
    horizontalScrollBar.addListener(this);

    timeAxisStrip = std::make_unique<TimeAxisStrip>(this);

    mediaAreaContainer.addAndMakeVisible(overheadPanel);
    mediaAreaContainer.addAndMakeVisible(contentComponent);
    mediaAreaContainer.addAndMakeVisible(*timeAxisStrip);
    mediaAreaContainer.addAndMakeVisible(horizontalScrollBar);
    addAndMakeVisible(mediaAreaContainer);

    mediaAreaFlexBox.flexDirection = FlexBox::Direction::column;

    currentPositionCursor.setFill(cursorColor);
    addAndMakeVisible(currentPositionCursor);

    resetPaths();
    resetScrollBar();
}

void MediaDisplayComponent::initializeButtons()
{
    // Mode when a playable file is loaded
    playButtonActiveInfo =
        MultiButton::Mode { "Play-Active",       "Click to start playback.",
                            [this] { start(); }, MultiButton::DrawingMode::IconOnly,
                            Colours::limegreen,  fontaudio::Play };
    // Mode when there is nothing to play
    playButtonInactiveInfo =
        MultiButton::Mode { "Play-Inactive",    "Nothing to play.",
                            [this] {},          MultiButton::DrawingMode::IconOnly,
                            Colours::lightgrey, fontaudio::Play };
    // Mode during playback
    stopButtonInfo = MultiButton::Mode { "Stop",
                                         "Click to stop playback.",
                                         [this] { stop(); },
                                         MultiButton::DrawingMode::IconOnly,
                                         Colours::orangered,
                                         fontaudio::Stop };
    playStopButton.addMode(playButtonActiveInfo);
    playStopButton.addMode(playButtonInactiveInfo);
    playStopButton.addMode(stopButtonInfo);
    headerComponent.addAndMakeVisible(playStopButton);

    chooseFileButtonActiveInfo = MultiButton::Mode { "ChooseFile",
                                                     "Click to choose a media file.",
                                                     [this] { chooseFileCallback(); },
                                                     MultiButton::DrawingMode::IconOnly,
                                                     Colours::lightblue,
                                                     fontawesome::Folder };
    chooseFileButtonInactiveInfo = MultiButton::Mode { "ChooseFile-Inactive",
                                                       "Cannot choose file while processing.",
                                                       [this] {},
                                                       MultiButton::DrawingMode::IconOnly,
                                                       Colours::lightgrey,
                                                       fontawesome::Folder };
    chooseFileButton.addMode(chooseFileButtonActiveInfo);
    chooseFileButton.addMode(chooseFileButtonInactiveInfo);
    headerComponent.addAndMakeVisible(chooseFileButton);

    // Mode when an unsaved file is loaded
    saveFileButtonActiveInfo = MultiButton::Mode { "Save-Active",
                                                   "Click to save the media file.",
                                                   [this] { saveFileCallback(); },
                                                   MultiButton::DrawingMode::IconOnly,
                                                   Colours::lightblue,
                                                   fontawesome::Save };
    // Mode when there is nothing to save
    saveFileButtonInactiveInfo =
        MultiButton::Mode { "Save-Inactive",    "Nothing to save.",
                            [this] {},          MultiButton::DrawingMode::IconOnly,
                            Colours::lightgrey, fontawesome::Save };
    saveFileButton.addMode(saveFileButtonActiveInfo);
    saveFileButton.addMode(saveFileButtonInactiveInfo);
    headerComponent.addAndMakeVisible(saveFileButton);

    resetButtonState();
}

MediaDisplayComponent::~MediaDisplayComponent()
{
    deviceManager.removeAudioCallback(&sourcePlayer);

    sourcePlayer.setSource(nullptr);

    headerComponent.removeMouseListener(this);
    horizontalScrollBar.removeListener(this);

    //clearLabels(); // Seems to cause problems when re-loading model
}

StringArray MediaDisplayComponent::getSupportedExtensions()
{
    StringArray audioExtensions = AudioDisplayComponent::getSupportedExtensions();
    StringArray midiExtensions = MidiDisplayComponent::getSupportedExtensions();

    StringArray allExtensions = StringArray(audioExtensions);
    allExtensions.mergeArray(midiExtensions);

    return allExtensions;
}

void MediaDisplayComponent::paint(Graphics& g)
{
    if (isThumbnailTrack() && isCurrentlySelected())
    {
        g.fillAll(selectionColor);
    }
    else
    {
        g.fillAll(defaultColor);
    }

    g.setColour(graphicsColor);

    if (! isFileLoaded())
    {
        g.setFont(14.0f);

        String text;

        if (isInputTrack())
        {
            text = "No media file selected...";
        }
        else if (isOutputTrack())
        {
            text = "Track is currently empty...";
        }
        else
        {
            text = "";
        }

        g.drawFittedText(text, getLocalBounds(), Justification::centred, 2);
    }
}

void MediaDisplayComponent::resized()
{
    Rectangle<int> totalBounds = getLocalBounds();

    // Remove existing items in main flex
    mainFlexBox.items.clear();

    if (isThumbnailTrack())
    {
        // Place header over media
        mainFlexBox.flexDirection = FlexBox::Direction::column;
        // Fixed area for track label and buttons
        mainFlexBox.items.add(FlexItem(headerComponent)
                                  .withHeight(trackNameLabel.getFont().getHeight())
                                  .withMargin(1));
    }
    else if (isPreviewTrack())
    {
        // Place header over media
        mainFlexBox.flexDirection = FlexBox::Direction::column;
        mainFlexBox.items.add(FlexItem(headerComponent)
                                  .withHeight(trackNameLabel.getFont().getHeight())
                                  .withMargin(1));
    }
    else
    {
        // Place header beside media
        mainFlexBox.flexDirection = FlexBox::Direction::row;
        // Fixed area for track label and buttons
        mainFlexBox.items.add(FlexItem(headerComponent).withFlex(1).withMaxWidth(40).withMargin(4));
    }

    // Media area takes remaining space
    mainFlexBox.items.add(FlexItem(mediaAreaContainer).withFlex(8));

    mainFlexBox.performLayout(totalBounds);

    // Remove existing items in header flex
    headerFlexBox.items.clear();

    // Add track label to header flex
    headerFlexBox.items.add(FlexItem(trackNameLabel).withFlex(1).withMargin({ 0, 2, 0, 0 }));

    if (! isThumbnailTrack() && ! isPreviewTrack())
    {
        // Add buttons to header flex
        headerFlexBox.items.add(FlexItem(buttonsComponent).withFlex(2).withMargin({ 0, 0, 0, 1 }));
    }

    // Perform layout of controls inside header
    headerFlexBox.performLayout(headerComponent.getLocalBounds());

    Rectangle<float> labelBounds = trackNameLabel.getBounds().toFloat();
    float trackNameLabelWidth = labelBounds.getWidth();
    float trackNameLabelHeight = labelBounds.getHeight();

    if (! isThumbnailTrack() && ! isPreviewTrack())
    {
        Point<float> labelCenter = labelBounds.getCentre();
        // Rotate track name label 90 degrees
        trackNameLabel.setTransform(
            AffineTransform::rotation(-MathConstants<float>::halfPi, labelCenter.x, labelCenter.y));

        // Swap width and height
        trackNameLabelWidth = labelBounds.getHeight();
        trackNameLabelHeight = labelBounds.getWidth();

        // Update track name label bounds and position
        trackNameLabel.setBounds(
            labelBounds.withSize(trackNameLabelWidth, trackNameLabelHeight).toNearestInt());
    }

    // Center track name label within header
    float labelX = (labelBounds.getWidth() - trackNameLabelWidth) / 2.0f;
    float labelY = (labelBounds.getHeight() - trackNameLabelHeight) / 2.0f;
    trackNameLabel.setTopLeftPosition(static_cast<int>(labelX), static_cast<int>(labelY));

    // Remove existing items in button flex
    buttonsFlexBox.items.clear();

    // Add buttons to flex with equal height
    buttonsFlexBox.items.add(
        FlexItem(playStopButton).withHeight(22).withWidth(22).withMargin({ 2, 0, 2, 0 }));
    if (isInputTrack())
    {
        buttonsFlexBox.items.add(
            FlexItem(chooseFileButton).withHeight(22).withWidth(22).withMargin({ 2, 0, 2, 0 }));
    }
    if (isOutputTrack())
    {
        buttonsFlexBox.items.add(
            FlexItem(saveFileButton).withHeight(22).withWidth(22).withMargin({ 2, 0, 2, 0 }));
    }

    buttonsFlexBox.performLayout(buttonsComponent.getBounds());

    // Remove existing items in media flex
    mediaAreaFlexBox.items.clear();

    if (getNumOverheadLabels() > 0)
    {
        // Add overhead panel if there are labels to display
        mediaAreaFlexBox.items.add(FlexItem(overheadPanel)
                                       .withHeight(labelHeight + 2 * controlSpacing)
                                       .withMargin({ 0,
                                                     getVerticalControlsWidth(),
                                                     static_cast<float>(controlSpacing),
                                                     getMediaXPos() }));
    }
    else
    {
        overheadPanel.setBounds(0, 0, 0, 0);
    }

    // Media component takes remaining space
    mediaAreaFlexBox.items.add(FlexItem(contentComponent).withFlex(1));

    if (timeAxisStrip != nullptr)
    {
        timeAxisStrip->setVisible(horizontalScrollBar.isVisible());
        if (timeAxisStrip->isVisible())
        {
            mediaAreaFlexBox.items.add(FlexItem(*timeAxisStrip)
                                           .withHeight(timeAxisHeight)
                                           .withMargin({ 0,
                                                         getVerticalControlsWidth(),
                                                         static_cast<float>(controlSpacing),
                                                         getMediaXPos() }));
        }
    }

    if (horizontalScrollBar.isVisible())
    {
        // Add horizontal scrollbar with fixed height
        mediaAreaFlexBox.items.add(FlexItem(horizontalScrollBar)
                                       .withHeight(scrollBarSize + 2 * controlSpacing)
                                       .withMargin({ static_cast<float>(controlSpacing),
                                                     getVerticalControlsWidth(),
                                                     0,
                                                     getMediaXPos() }));
    }

    // Perform layout in media area
    mediaAreaFlexBox.performLayout(mediaAreaContainer.getLocalBounds());

    if (! isLabelRepositioningScheduled)
    {
        isLabelRepositioningScheduled = true;

        // Defer label repositioning until all layout passes are complete
        MessageManager::callAsync(
            [this]()
            {
                isLabelRepositioningScheduled = false;

                repositionLabels();
            });
    }
}

void MediaDisplayComponent::repositionLabels()
{
    if (! isFileLoaded())
    {
        return;
    }

    float mediaWidth = getMediaWidth();
    float mediaHeight = getMediaHeight();

    float minLabelWidth = 0.1f * mediaWidth;
    float maxLabelWidth = 0.2f * mediaWidth;

    auto positionLabels = [this, minLabelWidth, maxLabelWidth, mediaHeight](auto& labels)
    {
        for (auto l : labels)
        {
            if (l == nullptr)
                continue;

            float labelWidth = jmax(
                minLabelWidth,
                jmin(maxLabelWidth, l->getTextWidth() + 2.0f * static_cast<float>(textSpacing)));

            double labelStartTime = l->getTime();
            double labelStopTime = labelStartTime + l->getDuration();
            double labelCenterTime = labelStartTime + l->getDuration() / 2.0;

            float xPos =
                correctMediaXBounds(timeToMediaX(labelCenterTime) - labelWidth / 2.0f, labelWidth);
            float yPos = 1.0f;

            if (auto lo = dynamic_cast<LabelOverlayComponent*>(l))
            {
                yPos = lo->getRelativeY() * mediaHeight - static_cast<float>(labelHeight) / 2.0f;
                yPos = jmin(mediaHeight - static_cast<float>(labelHeight), jmax(0.0f, yPos));
                yPos = mediaYToDisplayY(yPos);
            }

            Rectangle<float> labelBounds(xPos, yPos, labelWidth, static_cast<float>(labelHeight));
            l->setBounds(labelBounds.toNearestInt());
            //l->toFront(true);

            float cursorRadius = cursorWidth / 2.0f;

            float leftLabelMarkerPos = static_cast<float>(
                correctMediaXBounds(timeToMediaX(labelStartTime) - cursorRadius, cursorWidth));
            l->setLeftMarkerBounds(
                Rectangle<float>(leftLabelMarkerPos, 0, cursorWidth, mediaHeight).toNearestInt());

            float rightLabelMarkerPos = static_cast<float>(
                correctMediaXBounds(timeToMediaX(labelStopTime) - cursorRadius, cursorWidth));
            l->setRightMarkerBounds(
                Rectangle<float>(rightLabelMarkerPos, 0, cursorWidth, mediaHeight).toNearestInt());

            float durationWidth = jmax(0.0f, rightLabelMarkerPos - leftLabelMarkerPos);
            l->setDurationFillBounds(
                Rectangle<float>(leftLabelMarkerPos + cursorRadius, 0, durationWidth, mediaHeight)
                    .toNearestInt());

            if (l->getIndex() == currentTempFileIdx)
            {
                l->setVisible(true);
            }
            else
            {
                l->setVisible(false);
            }
        }
    };

    positionLabels(overheadLabels);
    positionLabels(labelOverlays);
}

void MediaDisplayComponent::timerCallback()
{
    if (isPlaying())
    {
        updateCursorPosition();
    }
    else
    {
        stop();
    }
}

void MediaDisplayComponent::setTrackName(String name)
{
    trackName = name;

    trackNameLabel.setText(trackName, dontSendNotification);
}

void MediaDisplayComponent::setChooseFileButtonEnabled(bool enabled)
{
    chooseFileButton.setMode(enabled ? chooseFileButtonActiveInfo.displayLabel
                                     : chooseFileButtonInactiveInfo.displayLabel);
}

void MediaDisplayComponent::resetDisplay()
{
    clearLabels();
    resetMedia();
    resetPaths();
    resetScrollBar();
    resetButtonState();
}

void MediaDisplayComponent::resetPaths()
{
    originalFilePath = URL();

    tempFilePaths.clear();
    currentTempFileIdx = -1;
}

void MediaDisplayComponent::resetTransport()
{
    transportSource.stop();
    transportSource.setSource(nullptr);
}

void MediaDisplayComponent::resetScrollBar()
{
    horizontalZoomFactor = 1.0;
    horizontalScrollBar.setRangeLimits({ 0.0, 1.0 });
    horizontalScrollBar.setVisible(false);
}

void MediaDisplayComponent::resetButtonState()
{
    playStopButton.setMode(playButtonInactiveInfo.displayLabel);
    chooseFileButton.setMode(chooseFileButtonActiveInfo.displayLabel);
    saveFileButton.setMode(saveFileButtonInactiveInfo.displayLabel);
}

void MediaDisplayComponent::initializeDisplay(const URL& filePath)
{
    resetDisplay();

    setOriginalFilePath(filePath);
    updateDisplay(filePath);

    if (! isThumbnailTrack())
    {
        horizontalScrollBar.setVisible(true);
    }
    updateVisibleRange({ 0.0, getTotalLengthInSecs() });
    resized(); // Needed to display scrollbar after loading
}

void MediaDisplayComponent::updateDisplay(const URL& filePath)
{
    resetMedia();

    loadMediaFile(filePath);
    postLoadActions(filePath);

    currentPositionCursor.toFront(true);

    Range<double> range(0.0, getTotalLengthInSecs());

    horizontalScrollBar.setRangeLimits(range);

    playStopButton.setMode(playButtonActiveInfo.displayLabel);
    saveFileButton.setMode(saveFileButtonActiveInfo.displayLabel);
}

void MediaDisplayComponent::setOriginalFilePath(URL filePath)
{
    originalFilePath = filePath;

    //addNewTempFile();
}

/*void MediaDisplayComponent::addNewTempFile()
{
    // Prune any future files in chain before adding new temp file
    clearFutureTempFiles();

    int numTempFiles = tempFilePaths.size();

    // Obtain original file used to initialize display
    File originalFile = originalFilePath.getLocalFile();

    File targetFile; // File to copy to new temp file

    if (! numTempFiles)
    {
        // Copy original file if no temp files
        targetFile = originalFile;
    }
    else
    {
        // Otherwise copy most recent temp file
        targetFile = getTempFilePath().getLocalFile();
    }

    String tempDirectory =
        File::getSpecialLocation(File::SpecialLocationType::tempDirectory).getFullPathName();

    String targetFileName = originalFile.getFileNameWithoutExtension();
    String targetFileExtension = originalFile.getFileExtension();

    URL tempFilePath = URL(File(tempDirectory + "/HARP/" + targetFileName + "_"
                                + String(numTempFiles) + targetFileExtension));

    File tempFile = tempFilePath.getLocalFile();

    tempFile.getParentDirectory().createDirectory();

    if (! targetFile.copyFileTo(tempFile))
    {
        DBG_AND_LOG("MediaDisplayComponent::addNewTempFile: Failed to copy file "
            << targetFile.getFullPathName() << " to " << tempFile.getFullPathName() << ".");
    }
    else
    {
        DBG_AND_LOG("MediaDisplayComponent::addNewTempFile: Copied file "
            << targetFile.getFullPathName() << " to " << tempFile.getFullPathName() << ".");
    }

    tempFilePaths.add(tempFilePath);
    currentTempFileIdx++;
}

bool MediaDisplayComponent::iteratePreviousTempFile()
{
    if (currentTempFileIdx > 0)
    {
        currentTempFileIdx--;

        updateDisplay(getTempFilePath());

        return true;
    }
    else
    {
        return false;
    }
}

bool MediaDisplayComponent::iterateNextTempFile()
{
    if (currentTempFileIdx + 1 < tempFilePaths.size())
    {
        currentTempFileIdx++;

        updateDisplay(getTempFilePath());

        return true;
    }
    else
    {
        return false;
    }
}

void MediaDisplayComponent::clearFutureTempFiles()
{
    int n = tempFilePaths.size() - (currentTempFileIdx + 1);

    tempFilePaths.removeLast(n);

    clearLabels(currentTempFileIdx + 1);
}

void MediaDisplayComponent::overwriteOriginalFile()
{
    File targetFile = originalFilePath.getLocalFile();
    File tempFile = getTempFilePath().getLocalFile();

    String parentDirectory = targetFile.getParentDirectory().getFullPathName();
    String targetFileName = targetFile.getFileNameWithoutExtension();
    String targetFileExtension = targetFile.getFileExtension();

    File backupFile =
        File(parentDirectory + "/" + targetFileName + "_BACKUP" + targetFileExtension);

    if (targetFile.copyFileTo(backupFile))
    {
        DBG_AND_LOG("MediaDisplayComponent::overwriteOriginalFile: Created backup of file "
            << targetFile.getFullPathName() << " at " << backupFile.getFullPathName() << ".");
    }
    else
    {
        DBG_AND_LOG("MediaDisplayComponent::overwriteOriginalFile: Failed to create backup of file "
            << targetFile.getFullPathName() << " at " << backupFile.getFullPathName() << ".");
    }

    if (tempFile.copyFileTo(targetFile))
    {
        DBG_AND_LOG("MediaDisplayComponent::overwriteOriginalFile: Overwriting file "
            << targetFile.getFullPathName() << " with " << tempFile.getFullPathName() << ".");
    }
    else
    {
        DBG_AND_LOG("MediaDisplayComponent::overwriteOriginalFile: Failed to overwrite file "
            << targetFile.getFullPathName() << " with " << tempFile.getFullPathName() << ".");
    }
}*/

bool MediaDisplayComponent::isDuplicateFile(const URL& filePath)
{
    return getOriginalFilePath()
           == filePath; //|| (isFileLoaded() && getTempFilePath() == filePath);
}

void MediaDisplayComponent::filesDropped(const StringArray& files, int /*x*/, int /*y*/)
{
    for (int i = 1; i < files.size(); i++)
    {
        DBG_AND_LOG("MediaDisplayComponent::filesDropped: Ignoring additional file " << files[i]
                                                                                     << ".");
    }

    File mediaFile = File(files[0]);

    if (isDuplicateFile(URL(mediaFile)))
    {
        DBG_AND_LOG("MediaDisplayComponent::filesDropped: Ignoring self-drag.");
        return;
    }

    if (! getInstanceExtensions().contains(mediaFile.getFileExtension()))
    {
        AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                         "Invalid File",
                                         "This display supports the following file types: "
                                             + getInstanceExtensions().joinIntoString(", ") + ".",
                                         "OK");
    }
    else
    {
        initializeDisplay(URL(mediaFile));
    }
}

void MediaDisplayComponent::chooseFileCallback()
{
    StringArray validExtensions = StringArray(getInstanceExtensions());
    String filePatternsAllowed = "*" + validExtensions.joinIntoString(";*");

    chooseFileBrowser =
        std::make_unique<FileChooser>("Select a media file...", File(), filePatternsAllowed);

    chooseFileBrowser->launchAsync(FileBrowserComponent::openMode
                                       | FileBrowserComponent::canSelectFiles,
                                   [this](const FileChooser& fc)
                                   {
                                       File chosenFile = fc.getResult();
                                       if (chosenFile != File {})
                                       {
                                           initializeDisplay(URL(chosenFile));
                                       }
                                   });
}

void MediaDisplayComponent::saveFileCallback()
{
    if (saveFileButton.getModeName() == saveFileButtonActiveInfo.displayLabel)
    {
        //overwriteOriginalFile();
        //saveFileButton.setMode(saveButtonInactiveInfo.displayLabel);

        /*if (statusBox != nullptr)
        {
            statusBox->setStatusMessage("File saved successfully");
        }*/

        if (isFileLoaded())
        {
            StringArray validExtensions = StringArray(getInstanceExtensions());
            String filePatternsAllowed = "*" + validExtensions.joinIntoString(";*");

            saveFileBrowser = std::make_unique<FileChooser>(
                "Select a save path...", getOriginalFilePath().getLocalFile(), filePatternsAllowed);

            saveFileBrowser->launchAsync(
                FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles,
                [this, validExtensions](const FileChooser& fc)
                {
                    File chosenFile = fc.getResult();
                    if (chosenFile != File {})
                    {
                        if (chosenFile.getFileExtension().compare("") == 0)
                        {
                            // Add default extension in none provided
                            chosenFile = chosenFile.withFileExtension(validExtensions[0]);
                        }

                        if (validExtensions.contains(chosenFile.getFileExtension()))
                        {
                            //URL tempFilePath = mediaDisplay->getTempFilePath();

                            if (URL(chosenFile) != getOriginalFilePath())
                            {
                                // Remove DAW linking
                                unlinkFromDAW();
                            }

                            // Attempt to save file contained within media display to chosen location
                            //bool saveSuccessful = tempFilePath.getLocalFile().copyFileTo(newFile);
                            if (getOriginalFilePath().getLocalFile().copyFileTo(chosenFile))
                            {
                                //loadMediaDisplay(newFile);

                                // Update path associated with media display
                                setOriginalFilePath(URL(chosenFile));

                                //saveFileButton.setMode(saveButtonInactiveInfo.label);

                                if (statusMessage != nullptr)
                                {
                                    statusMessage->setMessage("File successfully saved to "
                                                              + chosenFile.getFullPathName());
                                }
                            }
                            else
                            {
                                AlertWindow::showMessageBoxAsync(
                                    AlertWindow::WarningIcon,
                                    "Save Failed",
                                    "Failed to save file to " + chosenFile.getFullPathName() + ".",
                                    "OK");
                            }
                        }
                        else
                        {
                            AlertWindow::showMessageBoxAsync(
                                AlertWindow::WarningIcon,
                                "Invalid Extension",
                                "File must be saved with one of the following file extensions: "
                                    + validExtensions.joinIntoString(", ") + ".",
                                "OK");
                        }
                    }
                    else
                    {
                        //DBG_AND_LOG("MediaDisplayComponent::saveFileCallback: Save operation canceled.");
                    }
                });
        }
    }
}

float MediaDisplayComponent::getPixelsPerSecond()
{
    if (visibleRange.getLength())
    {
        return getMediaWidth() / static_cast<float>(visibleRange.getLength());
    }
    else
    {
        return 0.0f;
    }
}

double MediaDisplayComponent::mediaXToTime(const float mX)
{
    if (visibleRange.getLength())
    {
        return static_cast<double>(mX / getPixelsPerSecond()) + getTimeAtOrigin();
    }
    else
    {
        return 0.0;
    }
}

float MediaDisplayComponent::timeToMediaX(const double t)
{
    double t_ = jmin(getTotalLengthInSecs(), jmax(0.0, t));

    if (visibleRange.getLength())
    {
        return static_cast<float>(t_ - getTimeAtOrigin()) * getPixelsPerSecond();
    }
    else
    {
        return 0.0f;
    }
}

float MediaDisplayComponent::mediaXToDisplayX(const float mX)
{
    float offsetX = 0;
    float visibleStartX = 0;

    if (visibleRange.getLength())
    {
        offsetX = static_cast<float>(getTimeAtOrigin()) * getPixelsPerSecond();
        visibleStartX = static_cast<float>(visibleRange.getStart() * getPixelsPerSecond());
    }

    float dX = getMediaXPos() + mX - (visibleStartX - offsetX);

    return dX;
}

int MediaDisplayComponent::correctMediaXBounds(float mX, float width)
{
    mX = jmax(timeToMediaX(0.0), mX);
    mX = jmin(timeToMediaX(getTotalLengthInSecs()) - width, mX);

    return static_cast<int>(mX);
}

void MediaDisplayComponent::updateVisibleRange(Range<double> r)
{
    visibleRange = r;

    horizontalScrollBar.setCurrentRange(visibleRange);
    updateCursorPosition();
    repositionLabels();

    if (timeAxisStrip != nullptr)
        timeAxisStrip->repaint();

    visibleRangeCallback();
}

void MediaDisplayComponent::horizontalMove(double deltaT)
{
    double visibleStart = visibleRange.getStart();
    double visibleLength = visibleRange.getLength();

    const double totalLength = getTotalLengthInSecs();
    const double maxStart = jmax(0.0, totalLength - visibleLength);
    double newStart = visibleStart - deltaT * visibleLength / 10.0;
    newStart = jlimit(0.0, maxStart, newStart);

    updateVisibleRange({ newStart, newStart + visibleLength });
}

bool MediaDisplayComponent::horizontalZoom(double deltaZoom, double scrollPosT)
{
    const float mediaWidth = getMediaWidth();
    const double totalLength = getTotalLengthInSecs();

    if (mediaWidth <= 0.0f || totalLength <= 0.0)
        return false;

    const float pps = getPixelsPerSecond();
    if (pps <= 0.0f)
        return false;

    const double minVisibleSeconds = 5.0;
    const float minPps = static_cast<float>(mediaWidth / totalLength);
    float maxPps = static_cast<float>(mediaWidth / minVisibleSeconds);
    if (maxPps < minPps)
        maxPps = minPps;

    float newPps = pps * (1.0f + 0.5f * static_cast<float>(deltaZoom));
    newPps = jlimit(minPps, maxPps, newPps);

    if (std::abs(newPps - pps) < 0.01f)
        return false;

    double newVisibleLength = static_cast<double>(mediaWidth) / static_cast<double>(newPps);
    if (newVisibleLength > totalLength)
        newVisibleLength = totalLength;

    const double maxStart = jmax(0.0, totalLength - newVisibleLength);
    double newStart = scrollPosT - newVisibleLength * 0.5;
    newStart = jlimit(0.0, maxStart, newStart);
    double newEnd = newStart + newVisibleLength;

    updateVisibleRange({ newStart, newEnd });
    return true;
}

void MediaDisplayComponent::scrollBarMoved(ScrollBar* scrollBarThatHasMoved,
                                           double scrollBarRangeStart)
{
    if (scrollBarThatHasMoved == &horizontalScrollBar)
    {
        updateVisibleRange(visibleRange.movedToStartAt(scrollBarRangeStart));
    }
}

void MediaDisplayComponent::mouseWheelMove(const MouseEvent& evt, const MouseWheelDetails& wheel)
{
    if (isThumbnailTrack())
    {
        Component::mouseWheelMove(evt, wheel);
    }
    else if (isFileLoaded())
    {
#if (JUCE_MAC)
        bool commandMod = evt.mods.isCommandDown() || evt.mods.isCtrlDown();
#else
        bool commandMod = evt.mods.isCtrlDown();
#endif
        bool shiftMod = evt.mods.isShiftDown();

        double scrollTime = mediaXToTime(evt.position.getX());

        if (! commandMod)
        {
            if (std::abs(wheel.deltaX) > 2 * std::abs(wheel.deltaY))
            {
                // Horizontal scroll when using 2-finger swipe in macbook trackpad
                horizontalMove(static_cast<double>(wheel.deltaX));
            }
            else if (std::abs(wheel.deltaY) > 2 * std::abs(wheel.deltaX))
            {
                if (shiftMod)
                {
                    horizontalMove(static_cast<double>(wheel.deltaY));
                }
                else
                {
                    horizontalZoom(static_cast<double>(wheel.deltaY), scrollTime);
                }
            }
            else
            {
                // Do nothing
            }
        }
    }
    else
    {
        // Ignore mouse wheel events
    }
}

void MediaDisplayComponent::unlinkFromDAW()
{
    if (isLinkedToDAW())
    {
        linkedToDAW = false;

        if (isCurrentlySelected())
        {
            headerComponent.setColor(selectionColor);
        }
        else
        {
            headerComponent.setColor(defaultColor);
        }

        repaint();
    }
}

void MediaDisplayComponent::selectTrack()
{
    if (! isCurrentlySelected())
    {
        isSelected = true;

        if (! isLinkedToDAW())
        {
            headerComponent.setColor(selectionColor);
        }

        repaint();

        sendSynchronousChangeMessage();
    }
}

void MediaDisplayComponent::deselectTrack()
{
    if (isCurrentlySelected())
    {
        isSelected = false;

        if (! isLinkedToDAW())
        {
            headerComponent.setColor();
        }

        repaint();

        sendSynchronousChangeMessage();
    }
}

void MediaDisplayComponent::start()
{
    startPlaying();

    startTimerHz(40);

    playStopButton.setMode(stopButtonInfo.displayLabel);
}

void MediaDisplayComponent::stop()
{
    stopPlaying();

    stopTimer();

    currentPositionCursor.setVisible(false);
    setPlaybackPosition(0.0);

    playStopButton.setMode(playButtonActiveInfo.displayLabel);

    sendChangeMessage();
}

void MediaDisplayComponent::updateCursorPosition()
{
    float mediaWidth = getMediaWidth();
    float mediaXOffset = mediaXToDisplayX(0.0f);
    float minCursorXPos = mediaXToDisplayX(timeToMediaX(visibleRange.getStart()));
    float maxCursorXPos = mediaXOffset + mediaWidth;

    float cursorPositionX = mediaXToDisplayX(timeToMediaX(getPlaybackPosition()));
    float cursorPositionY = 0;

    if (isPlaying() && cursorPositionX >= minCursorXPos && cursorPositionX <= maxCursorXPos)
    {
        currentPositionCursor.setVisible(! isThumbnailTrack());
    }
    else if (isFileLoaded() && ! isPlaying()
             && (getMediaComponent()->isMouseButtonDown(false)
                 && getLocalBounds().contains(getMouseXYRelative())))
    {
        currentPositionCursor.setVisible(! isThumbnailTrack());
    }
    else
    {
        currentPositionCursor.setVisible(false);
    }

    cursorPositionX = jmin(maxCursorXPos, jmax(minCursorXPos, cursorPositionX));

    Rectangle<int> mediaAreaBounds = mediaAreaContainer.getBounds();
    Rectangle<int> mediaBounds = contentComponent.getBounds();

    // Include offset(s) for track header
    cursorPositionX += static_cast<float>(mediaAreaBounds.getX());
    cursorPositionY += static_cast<float>(mediaAreaBounds.getY());
    // Include offset for label overlay header
    cursorPositionY += static_cast<float>(mediaBounds.getY());
    // Offset by half of width
    cursorPositionX -= cursorWidth / 2.0f;

    currentPositionCursor.setRectangle(
        Rectangle<float>(cursorPositionX, cursorPositionY, cursorWidth, mediaBounds.getHeight()));
}

Rectangle<int> MediaDisplayComponent::getChooseFileButtonBounds()
{
    if (auto* p = chooseFileButton.getParentComponent())
    {
        return getLocalArea(p, chooseFileButton.getBounds());
    }

    return chooseFileButton.getBounds();
}

Rectangle<int> MediaDisplayComponent::getPlayButtonBounds()
{
    if (auto* p = playStopButton.getParentComponent())
    {
        return getLocalArea(p, playStopButton.getBounds());
    }

    return playStopButton.getBounds();
}

void MediaDisplayComponent::mouseEnter(const MouseEvent& e)
{
    if (! isThumbnailTrack() && e.eventComponent == getMediaComponent()
        && instructionsMessage != nullptr)
    {
        instructionsMessage->setMessage(mediaInstructions);
    }
}

void MediaDisplayComponent::mouseExit(const MouseEvent& e)
{
    if (! isThumbnailTrack() && e.eventComponent == getMediaComponent()
        && instructionsMessage != nullptr)
    {
        instructionsMessage->clearMessage();
    }
}

void MediaDisplayComponent::mouseDown(const MouseEvent& e)
{
    mouseDrag(e); // Make sure playback position has been updated

    if (isThumbnailTrack() && isFileLoaded())
    {
        selectTrack();
    }
}

void MediaDisplayComponent::mouseDrag(const MouseEvent& e)
{
    if (isFileLoaded())
    {
        if (! isThumbnailTrack() && e.eventComponent == getMediaComponent() && ! isPlaying()
            && getLocalBounds().contains(getMouseXYRelative()))
        {
            float x_ = static_cast<float>(e.x);

            double visibleStart = visibleRange.getStart();
            double visibleStop = visibleStart + visibleRange.getLength();

            x_ = jmax(timeToMediaX(visibleStart), x_);
            x_ = jmin(timeToMediaX(visibleStop), x_);

            setPlaybackPosition(mediaXToTime(x_));
        }

        if (! getLocalBounds().contains(getMouseXYRelative()))
        {
            //performExternalDragDropOfFiles(
            //    StringArray(getTempFilePath().getLocalFile().getFullPathName()), true, this);
            performExternalDragDropOfFiles(
                StringArray(getOriginalFilePath().getLocalFile().getFullPathName()), true, this);
        }

        updateCursorPosition();
    }
}

void MediaDisplayComponent::mouseUp(const MouseEvent& e)
{
    mouseDrag(e); // Make sure playback position has been updated

    if (! isThumbnailTrack())
    {
        if (e.eventComponent == getMediaComponent() && isFileLoaded() && isMouseOver(true))
        {
            start(); // Only start playback if still within media area
        }
        else
        {
            if (! isPlaying())
            {
                setPlaybackPosition(0.0);
            }
        }
    }
}

void MediaDisplayComponent::mouseDoubleClick(const MouseEvent& e)
{
    // TODO - mouseUp/Down (selectTrack()) is still called before this

    if (isThumbnailTrack() && isFileLoaded() && isMouseOver(true))
    {
        deselectTrack();
    }
}

int MediaDisplayComponent::getNumOverheadLabels()
{
    int nOverheadLabels = 0;

    for (auto l : overheadLabels)
    {
        if (l->getIndex() == currentTempFileIdx)
        {
            nOverheadLabels++;
        }
    }

    return nOverheadLabels;
}

void MediaDisplayComponent::addOverheadLabel(OverheadLabelComponent* l)
{
    l->setFont(Font(jmax(minFontSize, labelHeight - 2 * textSpacing)));
    l->setIndex(currentTempFileIdx);
    overheadLabels.add(l);

    Component* mediaComponentPtr = getMediaComponent();
    l->addMarkersTo(mediaComponentPtr);
    overheadPanel.addAndMakeVisible(l);

    resized(); // Needed to make panel and labels visible
}

void MediaDisplayComponent::removeOverheadLabel(OverheadLabelComponent* l)
{
    Component* mediaComponentPtr = getMediaComponent();

    l->removeMarkersFrom(mediaComponentPtr);
    overheadPanel.removeChildComponent(l);

    overheadLabels.removeObject(l);
}

void MediaDisplayComponent::addLabelOverlay(LabelOverlayComponent* l)
{
    l->setFont(Font(jmax(minFontSize, labelHeight - 2 * textSpacing)));
    l->setIndex(currentTempFileIdx);
    labelOverlays.add(l);

    Component* mediaComponentPtr = getMediaComponent();
    l->addMarkersTo(mediaComponentPtr);
    mediaComponentPtr->addAndMakeVisible(l);

    repositionLabels(); // Needed to make labels visible
}

void MediaDisplayComponent::removeLabelOverlay(LabelOverlayComponent* l)
{
    Component* mediaComponentPtr = getMediaComponent();

    l->removeMarkersFrom(mediaComponentPtr);
    mediaComponentPtr->removeChildComponent(l);

    labelOverlays.removeObject(l);
}

void MediaDisplayComponent::addLabels(const LabelList& labels)
{
    for (const auto& l : labels)
    {
        if (! shouldRenderLabel(l))
        {
            continue;
        }

        std::unique_ptr<OutputLabelComponent> lc =
            std::make_unique<OutputLabelComponent>((double) l->t, l->label);

        if ((l->description).has_value())
        {
            lc->setDescription((l->description).value());
        }

        if ((l->duration).has_value())
        {
            lc->setDuration(static_cast<double>((l->duration).value()));
        }

        if ((l->color).has_value())
        {
            lc->setColor(Colour(static_cast<uint32_t>((l->color).value())));
        }

        if ((l->link).has_value())
        {
            lc->setLink((l->link).value());
        }

        float y = 0.0f;

        bool isOverlay = false;

        if (auto audioLabel = dynamic_cast<AudioLabel*>(l.get()))
        {
            if ((audioLabel->amplitude).has_value())
            {
                isOverlay = true;

                float amp = (audioLabel->amplitude).value();

                y = LabelOverlayComponent::amplitudeToRelativeY(amp);
            }
        }

        if (auto midiLabel = dynamic_cast<MidiLabel*>(l.get()))
        {
            if ((midiLabel->pitch).has_value())
            {
                isOverlay = true;

                float p = (midiLabel->pitch).value();

                y = LabelOverlayComponent::pitchToRelativeY(p);
            }
        }

        if (isOverlay)
        {
            auto* lo = new LabelOverlayComponent(*static_cast<LabelOverlayComponent*>(lc.get()));
            lo->setRelativeY(y);

            addLabelOverlay(lo);
        }
        else
        {
            auto* ol = new OverheadLabelComponent(*static_cast<OverheadLabelComponent*>(lc.get()));
            addOverheadLabel(ol);
        }
    }
}

void MediaDisplayComponent::clearLabels(int processingIdxCutoff)
{
    for (int i = labelOverlays.size() - 1; i >= 0; --i)
    {
        LabelOverlayComponent* l = labelOverlays[i];

        if (l->getIndex() >= processingIdxCutoff)
        {
            removeLabelOverlay(l);
        }
    }

    if (! processingIdxCutoff)
    {
        labelOverlays.clear();
    }

    for (int i = overheadLabels.size() - 1; i >= 0; --i)
    {
        OverheadLabelComponent* l = overheadLabels[i];

        if (l->getIndex() >= processingIdxCutoff)
        {
            removeOverheadLabel(l);
        }
    }

    if (! processingIdxCutoff)
    {
        overheadLabels.clear();
    }

    resized(); // Remove overhead label panel
}