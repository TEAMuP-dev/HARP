#include "MediaDisplayComponent.h"

MediaDisplayComponent::MediaDisplayComponent()
{
    resetPaths();

    formatManager.registerBasicFormats();

    deviceManager.initialise(0, 2, nullptr, true, {}, nullptr);
    deviceManager.addAudioCallback(&sourcePlayer);

    sourcePlayer.setSource(&transportSource);

    addChildComponent(horizontalScrollBar);
    horizontalScrollBar.setAutoHide(false);
    horizontalScrollBar.addListener(this);

    currentPositionMarker.setFill(Colours::white.withAlpha(0.85f));
    addAndMakeVisible(currentPositionMarker);


    textLabel.setText("Media Info", juce::dontSendNotification);
    button1.setButtonText("Button 1");
    button1.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    // button2.setButtonText("Button 2");
    // button3.setButtonText("Button 3");

    // leftPanelFlexBox.items.add(juce::FlexItem(textLabel));
    // leftPanelFlexBox.items.add(juce::FlexItem(button1));
    // leftPanelFlexBox.items.add(juce::FlexItem(button2).withMinHeight(30.0f).withFlex(1.0f));
    // leftPanelFlexBox.items.add(juce::FlexItem(button3).withMinHeight(30.0f).withFlex(1.0f));

    // addAndMakeVisible(textLabel);
    // addAndMakeVisible(button1);
    // addAndMakeVisible(button2);
    // addAndMakeVisible(button3);

    addAndMakeVisible(controlBox);
    addAndMakeVisible(mediaBox);

    // Add controls to controlBox
    controlBox.addAndMakeVisible(textLabel);
    controlBox.addAndMakeVisible(button1);
    // controlBox.addAndMakeVisible(button2);
    // controlBox.addAndMakeVisible(button3);
}

MediaDisplayComponent::~MediaDisplayComponent()
{
    deviceManager.removeAudioCallback(&sourcePlayer);

    sourcePlayer.setSource(nullptr);

    horizontalScrollBar.removeListener(this);

    clearLabels();
}

void MediaDisplayComponent::paint(Graphics& g)
{
    g.fillAll(Colours::darkgrey);
    g.setColour(Colours::lightblue);

    if (! isFileLoaded())
    {
        g.setFont(14.0f);
        g.drawFittedText("No media file selected...", getLocalBounds(), Justification::centred, 2);
    }
    jassert(button1.isVisible());
    jassert(button1.getWidth() > 0 && button1.getHeight() > 0);
}

void MediaDisplayComponent::resized()
{
    auto totalBounds = getLocalBounds();

    // Build trackRowBox items
    trackRowFlexBox.items.clear();
    trackRowFlexBox.items.add(juce::FlexItem(controlBox).withWidth(200)); // Fixed width for controlBox
    trackRowFlexBox.items.add(juce::FlexItem(mediaBox).withFlex(1));      // Media area takes remaining space

    trackRowFlexBox.performLayout(totalBounds);

    // Set up controlFlexBox for the controls inside controlBox
    controlFlexBox.flexDirection = juce::FlexBox::Direction::column;
    controlFlexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    controlFlexBox.alignItems = juce::FlexBox::AlignItems::stretch;

    controlFlexBox.items.clear();
    controlFlexBox.items.add(juce::FlexItem(textLabel).withHeight(30));
    controlFlexBox.items.add(juce::FlexItem(button1).withHeight(30));
    controlFlexBox.items.add(juce::FlexItem(button2).withHeight(30));
    controlFlexBox.items.add(juce::FlexItem(button3).withHeight(30));

    // Perform layout of controls inside controlBox
    controlFlexBox.performLayout(controlBox.getLocalBounds());
    
    
    // juce::FlexBox trackRow;
    // trackRowBox.flexDirection = juce::FlexBox::Direction::row;
    // trackRowBox.justifyContent = juce::FlexBox::JustifyContent::center;
    // trackRowBox.alignItems = juce::FlexBox::AlignItems::center;

    
    // // Remove top area for control spacing
    // totalBounds.removeFromTop(controlSpacing);

    // // Remove bottom area for scrollbar and spacing
    // totalBounds.removeFromBottom(scrollBarSize + controlSpacing);

    // Get the full bounds
    // auto totalBounds = getLocalBounds();

    // int leftPanelWidth = jmin(0.2f * totalBounds.getWidth(), 200.0f);
    // Get bounds for the left panel
    // auto leftPanelBounds = totalBounds.removeFromLeft(leftPanelWidth);

    // Layout the left panel components using FlexBox
    // Initialize FlexBox and add components
    // controlBox.flexDirection = juce::FlexBox::Direction::column;
    // controlBox.justifyContent = juce::FlexBox::JustifyContent::center;
    // controlBox.alignItems = juce::FlexBox::AlignItems::center;

    // controlBox.items.add(juce::FlexItem(textLabel).withFlex(1));
    // controlBox.items.add(juce::FlexItem(button1).withFlex(1));
    

    // trackRowBox.items.add(juce::FlexItem(controlBox).withFlex(1));
    // // trackRowBox.items.add(juce::FlexItem(*this).withFlex(4));
    // trackRowBox.items.add(juce::FlexItem(mediaBox).withFlex(2));

    // trackRowBox.performLayout(totalBounds);

    // jassert(button1.isVisible());
    // jassert(button1.getWidth() > 0 && button1.getHeight() > 0);

    // button1.setBounds(10, 10, 100, 30);
    // repositionContent();
    repositionScrollBar();
    repositionLabels();
}

// Rectangle<int> MediaDisplayComponent::getContentBounds()
// {
//     return getLocalBounds()
//         .removeFromTop(getHeight() - (scrollBarSize + 2 * controlSpacing))
//         .reduced(controlSpacing);
// }

Rectangle<int> MediaDisplayComponent::getContentBounds()
{

    // // Start with the local bounds
    auto bounds = getLocalBounds();
    // auto leftPanelWidth = jmin(0.1f * bounds.getWidth(), 200.0f);
    // // Remove the left panel area from the bounds
    // bounds.removeFromLeft(leftPanelWidth);

    // // Now apply your existing calculations
    // bounds = bounds
    //     .removeFromTop(getHeight() - (scrollBarSize + 2 * controlSpacing))
    //     .reduced(controlSpacing);

    // auto bounds = mediaBox.
    return bounds;
}

void MediaDisplayComponent::repositionScrollBar()
{
    horizontalScrollBar.setBounds(getLocalBounds()
                                      .removeFromBottom(scrollBarSize + 2 * controlSpacing)
                                      .reduced(controlSpacing));
}

void MediaDisplayComponent::repositionOverheadLabels()
{
    // for (auto l : oveheadLabels) {}
}

void MediaDisplayComponent::repositionLabelOverlays()
{
    if (! visibleRange.getLength())
    {
        return;
    }

    float mediaHeight = getMediaHeight();
    float mediaWidth = getMediaWidth();

    float pixelsPerSecond = mediaWidth / visibleRange.getLength();

    float minLabelWidth = 0.1 * getMediaWidth();
    float maxLabelWidth = 0.10 * pixelsPerSecond;

    float contentWidth = getContentBounds().getWidth();
    float minVisibilityWidth = contentWidth / 200.0f;
    float maxVisibilityWidth = contentWidth / 3.0f;

    minLabelWidth = jmin(minLabelWidth, maxVisibilityWidth);
    maxLabelWidth = jmax(maxLabelWidth, minVisibilityWidth);

    for (auto l : labelOverlays)
    {
        float textWidth = l->getFont().getStringWidthFloat(l->getText());
        float labelWidth = jmax(minLabelWidth, jmin(maxLabelWidth, textWidth + 2 * textSpacing));

        // TODO - l->getDuration() unused

        float xPos = timeToMediaX(l->getTime());
        float yPos = l->getRelativeY() * mediaHeight;

        xPos -= labelWidth / 2.0f;
        yPos -= labelHeight / 2.0f;

        xPos = jmax(timeToMediaX(0.0), xPos);
        xPos = jmin(timeToMediaX(getTotalLengthInSecs()) - labelWidth, xPos);
        yPos = jmin(mediaHeight - labelHeight, jmax(0.0f, yPos));

        l->setBounds(xPos, yPos, labelWidth, labelHeight);
    }
}

void MediaDisplayComponent::repositionLabels()
{
    repositionOverheadLabels();
    repositionLabelOverlays();
}

void MediaDisplayComponent::changeListenerCallback(ChangeBroadcaster*)
{
    repaint();
    resized();
}

void MediaDisplayComponent::resetMedia()
{
    resetPaths();
    clearLabels();
    resetDisplay();
    sendChangeMessage();

    currentHorizontalZoomFactor = 1.0;
    horizontalScrollBar.setRangeLimits({ 0.0, 1.0 });
    horizontalScrollBar.setVisible(false);
}

void MediaDisplayComponent::setupDisplay(const URL& filePath)
{
    resetMedia();

    setNewTarget(filePath);
    updateDisplay(filePath);

    horizontalScrollBar.setVisible(true);
    updateVisibleRange({ 0.0, getTotalLengthInSecs() });
}

void MediaDisplayComponent::updateDisplay(const URL& filePath)
{
    resetDisplay();

    loadMediaFile(filePath);
    postLoadActions(filePath);

    currentPositionMarker.toFront(true);

    Range<double> range(0.0, getTotalLengthInSecs());

    horizontalScrollBar.setRangeLimits(range);
}

void MediaDisplayComponent::addNewTempFile()
{
    clearFutureTempFiles();

    int numTempFiles = tempFilePaths.size();
    // TODO: for outputMediaDisplays there might not be a
    // targetFilePath yet, so we should handle that case
    // "originalFile" is the first file that was displayed 
    // in this mediaDisplay (either the first input)
    // or the first output)
    File originalFile;
    if (targetFilePath.isLocalFile())
        originalFile = targetFilePath.getLocalFile();        

    File targetFile;

    if (! numTempFiles)
    {
        targetFile = originalFile;
    }
    else
    {
        targetFile = getTempFilePath().getLocalFile();
    }

    String docsDirectory =
        File::getSpecialLocation(File::SpecialLocationType::userDocumentsDirectory)
            .getFullPathName();

    String targetFileName = originalFile.getFileNameWithoutExtension();
    String targetFileExtension = originalFile.getFileExtension();

    URL tempFilePath = URL(File(docsDirectory + "/HARP/" + targetFileName + "_"
                                + String(numTempFiles) + targetFileExtension));

    File tempFile = tempFilePath.getLocalFile();

    tempFile.getParentDirectory().createDirectory();

    if (! targetFile.copyFileTo(tempFile))
    {
        DBG("MediaDisplayComponent::generateTempFile: Failed to copy file "
            << targetFile.getFullPathName() << " to " << tempFile.getFullPathName() << ".");

        AlertWindow(
            "Error", "Failed to create temporary file for processing.", AlertWindow::WarningIcon);
    }
    else
    {
        DBG("MediaDisplayComponent::generateTempFile: Copied file "
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
}

void MediaDisplayComponent::overwriteTarget()
{
    // Overwrite the original file - necessary for seamless sample editing integration

    File targetFile = targetFilePath.getLocalFile();
    File tempFile = getTempFilePath().getLocalFile();

    String parentDirectory = targetFile.getParentDirectory().getFullPathName();
    String targetFileName = targetFile.getFileNameWithoutExtension();
    String targetFileExtension = targetFile.getFileExtension();

    File backupFile =
        File(parentDirectory + "/" + targetFileName + "_BACKUP" + targetFileExtension);

    if (targetFile.copyFileTo(backupFile))
    {
        DBG("MediaDisplayComponent::overwriteTarget: Created backup of file"
            << targetFile.getFullPathName() << " at " << backupFile.getFullPathName() << ".");
    }
    else
    {
        DBG("MediaDisplayComponent::overwriteTarget: Failed to create backup of file"
            << targetFile.getFullPathName() << " at " << backupFile.getFullPathName() << ".");
    }

    if (tempFile.copyFileTo(targetFile))
    {
        DBG("MediaDisplayComponent::overwriteTarget: Overwriting file "
            << targetFile.getFullPathName() << " with " << tempFile.getFullPathName() << ".");
    }
    else
    {
        DBG("MediaDisplayComponent::overwriteTarget: Failed to overwrite file "
            << targetFile.getFullPathName() << " with " << tempFile.getFullPathName() << ".");
    }
}

void MediaDisplayComponent::filesDropped(const StringArray& files, int /*x*/, int /*y*/)
{
    // TODO - warning or handling for additional files

    droppedFilePath = URL(File(files[0]));
    sendChangeMessage();
}

void MediaDisplayComponent::mouseDrag(const MouseEvent& e)
{
    if (e.eventComponent == getMediaComponent() && ! isPlaying())
    {
        float x_ = (float) e.x;

        double visibleStart = visibleRange.getStart();
        double visibleStop = visibleStart + visibleRange.getLength();

        x_ = jmax(timeToMediaX(visibleStart), x_);
        x_ = jmin(timeToMediaX(visibleStop), x_);

        setPlaybackPosition(mediaXToTime(x_));
        updateCursorPosition();
    }
}

void MediaDisplayComponent::mouseUp(const MouseEvent& e)
{
    mouseDrag(e); // make sure playback position has been updated

    for (OverheadLabelComponent* label : oveheadLabels)
    {
        if (label->isMouseOver()) {
            //TODO
        }
    }

    for (LabelOverlayComponent* label : labelOverlays)
    {   
        DBG("Checking label overlap");
        if (label->isMouseOver()) {
            String link = label->getLink();
            DBG("Attempting to load link " << link);
            if (link != "") {
                URL link_url = URL(link);
                if (!link_url.isWellFormed()) {
                    DBG("Link appears malformed: " << link);
                } else {
                    DBG("Opening link " << link);
                    link_url.launchInDefaultBrowser();
                    return;
                }
            }
        }
    }

    if (e.eventComponent == getMediaComponent())
    {
        start();
        sendChangeMessage();
    }
}

void MediaDisplayComponent::start()
{
    startPlaying();

    startTimerHz(40);
}

void MediaDisplayComponent::stop()
{
    stopPlaying();

    stopTimer();

    currentPositionMarker.setVisible(false);
    setPlaybackPosition(0.0);
}

float MediaDisplayComponent::getPixelsPerSecond()
{
    return getMediaWidth() / visibleRange.getLength();
}

void MediaDisplayComponent::updateVisibleRange(Range<double> r)
{
    visibleRange = r;

    horizontalScrollBar.setCurrentRange(visibleRange);
    updateCursorPosition();
    repositionLabels();
    repaint();
}

String MediaDisplayComponent::getMediaHandlerInstructions()
{
    String toolTipText = mediaHandlerInstructions;

    for (OverheadLabelComponent* label : oveheadLabels)
    {
        if (label->isMouseOver())
        {
            toolTipText = label->getDescription();
        }
    }

    for (LabelOverlayComponent* label : labelOverlays)
    {
        if (label->isMouseOver())
        {
            toolTipText = label->getDescription();
        }
    }

    return toolTipText;
}

void MediaDisplayComponent::addLabels(LabelList& labels)
{
    clearLabels();

    for (const auto& l : labels)
    {
        String lbl = l->label;
        String dsc = l->description;

        if (dsc.isEmpty())
        {
            dsc = lbl;
        }

        float dur = 0.0f;

        if ((l->duration).has_value())
        {
            dur = (l->duration).value();
        }

        Colour color = Colours::purple.withAlpha(0.8f);

        if ((l->color).has_value())
        {
            color = Colour((l->color).value());
        }

        if (! dynamic_cast<AudioLabel*>(l.get()) && ! dynamic_cast<SpectrogramLabel*>(l.get())
            && ! dynamic_cast<MidiLabel*>(l.get()))
        {
            // TODO - OverheadLabelComponent((double) l->t, lbl, (double) dur, dsc, color);
        }
    }
}

void MediaDisplayComponent::addLabelOverlay(LabelOverlayComponent l)
{
    LabelOverlayComponent* label = new LabelOverlayComponent(l);
    label->setFont(Font(jmax(minFontSize, labelHeight - 2 * textSpacing)));
    labelOverlays.add(label);

    getMediaComponent()->addAndMakeVisible(label);
}

void MediaDisplayComponent::addOverheadLabel(OverheadLabelComponent l)
{
    // TODO
}

void MediaDisplayComponent::removeOutputLabel(OutputLabelComponent* l)
{
    // TODO
}

void MediaDisplayComponent::clearLabels()
{
    Component* mediaComponent = getMediaComponent();

    for (int i = 0; i < labelOverlays.size(); i++)
    {
        LabelOverlayComponent* l = labelOverlays.getReference(i);
        mediaComponent->removeChildComponent(l);

        delete l;
    }

    labelOverlays.clear();

    /*for (int i = 0; i < oveheadLabels.size(); i++) {
        OverheadLabelComponent* l = oveheadLabels.getReference(i);
        mediaComponent->removeChildComponent(l);

        delete l;
    }*/

    oveheadLabels.clear();

    resized();
    repaint();
}

void MediaDisplayComponent::setNewTarget(URL filePath)
{
    targetFilePath = filePath;

    addNewTempFile();
}

double MediaDisplayComponent::mediaXToTime(const float x)
{
    float x_ = jmin(getMediaWidth(), jmax(0.0f, x));

    double t = ((double) (x_ / getPixelsPerSecond())) + getTimeAtOrigin();

    return t;
}

float MediaDisplayComponent::timeToMediaX(const double t)
{
    float x;

    if (visibleRange.getLength() <= 0)
    {
        x = 0;
    }
    else
    {
        double t_ = jmin(getTotalLengthInSecs(), jmax(0.0, t));

        x = ((float) (t_ - getTimeAtOrigin())) * getPixelsPerSecond();
    }

    return x;
}

float MediaDisplayComponent::mediaXToDisplayX(const float mX)
{
    float visibleStartX = visibleRange.getStart() * getPixelsPerSecond();
    float offsetX = ((float) getTimeAtOrigin()) * getPixelsPerSecond();

    float dX = controlSpacing + getMediaXPos() + mX - (visibleStartX - offsetX);

    return dX;
}

void MediaDisplayComponent::resetTransport()
{
    transportSource.stop();
    transportSource.setSource(nullptr);
}

void MediaDisplayComponent::horizontalMove(float deltaX)
{
    auto totalLength = visibleRange.getLength();
    auto visibleStart = visibleRange.getStart();
    // auto scrollTime = mediaXToTime(evt.position.getX());
    auto newStart = visibleStart - deltaX * totalLength / 10.0;
    newStart = jlimit(0.0, jmax(0.0, getTotalLengthInSecs() - totalLength), newStart);

    if (! isPlaying())
    {
        updateVisibleRange({ newStart, newStart + totalLength });
    }
}

void MediaDisplayComponent::horizontalZoom(float deltaZoom, float scrollPosX)
{
    auto totalLength = visibleRange.getLength();
    auto visibleStart = visibleRange.getStart();
    // auto scrollTime = mediaXToTime(evt.position.getX());
    currentHorizontalZoomFactor = jlimit(1.0, 1.99, currentHorizontalZoomFactor + deltaZoom);

    auto newScale = jmax(0.01, getTotalLengthInSecs() * (2 - currentHorizontalZoomFactor));

    auto newStart = scrollPosX - newScale * (scrollPosX - visibleStart) / totalLength;
    auto newEnd = scrollPosX + newScale * (visibleStart + totalLength - scrollPosX) / totalLength;

    updateVisibleRange({ newStart, newEnd });
}

void MediaDisplayComponent::resetPaths()
{
    clearDroppedFile();

    targetFilePath = URL();

    tempFilePaths.clear();
    currentTempFileIdx = -1;
}

// TODO - may be able to simplify some of this logic by embedding cursor in media component
void MediaDisplayComponent::updateCursorPosition()
{
    bool displayCursor =
        isFileLoaded() && (isPlaying() || getMediaComponent()->isMouseButtonDown(true));

    float cursorPositionX = mediaXToDisplayX(timeToMediaX(getPlaybackPosition()));

    Rectangle<int> mediaBounds = getContentBounds();

    float cursorBoundsStartX = mediaBounds.getX() + getMediaXPos();
    float cursorBoundsWidth = visibleRange.getLength() * getPixelsPerSecond();

    // TODO - due to very small differences, cursor may not be visible at media bounds when zoomed in
    if (cursorPositionX >= cursorBoundsStartX
        && cursorPositionX <= (cursorBoundsStartX + cursorBoundsWidth))
    {
        currentPositionMarker.setVisible(displayCursor);
    }
    else
    {
        currentPositionMarker.setVisible(false);
    }

    cursorPositionX -= cursorWidth / 2.0f;

    currentPositionMarker.setRectangle(Rectangle<float>(
        cursorPositionX, mediaBounds.getY(), cursorWidth, mediaBounds.getHeight()));
}

void MediaDisplayComponent::timerCallback()
{
    if (isPlaying())
    {
        updateVisibleRange(visibleRange);
    }
    else
    {
        stop();
        sendChangeMessage();
    }
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
    // DBG("Mouse wheel moved: deltaX=" << wheel.deltaX << ", deltaY=" << wheel.deltaY << ", scrollPos:" << evt.position.getX());

    if (getTotalLengthInSecs() > 0.0)
    {
        bool isCmdPressed = evt.mods.isCommandDown(); // Command key
        bool isShiftPressed = evt.mods.isShiftDown(); // Shift key
        bool isCtrlPressed = evt.mods.isCtrlDown(); // Control key

#if JUCE_MAC
        bool zoomMod = isCmdPressed;
#else
        bool zoomMod = isCtrlPressed;
#endif

        auto totalLength = visibleRange.getLength();
        auto visibleStart = visibleRange.getStart();
        auto scrollTime = mediaXToTime(evt.position.getX());
        DBG("Visible range: (" << visibleStart << ", " << visibleStart + totalLength
                               << ") Scrolled at time: " << scrollTime);

        if (std::abs(wheel.deltaX) > 2 * std::abs(wheel.deltaY))
        {
            // Horizontal scroll when using 2-finger swipe in macbook trackpad
            horizontalMove(wheel.deltaX);
        }
        else if (std::abs(wheel.deltaY) > 2 * std::abs(wheel.deltaX))
        {
            // if (wheel.deltaY != 0) {
            horizontalZoom(wheel.deltaY, scrollTime);

            // }
        }
        else
        {
            // Do nothing
        }

        repaint();
    }
}
