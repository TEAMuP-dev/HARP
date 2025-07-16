#pragma once

#include "juce_core/juce_core.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <juce_audio_utils/juce_audio_utils.h>

#include "../gui/MultiButton.h"
#include "../utils.h"
#include "OutputLabelComponent.h"

using namespace juce;

class OverheadPanel : public Component
{
public:
    void paint(Graphics& g) override { g.fillAll(Colours::darkgrey.darker()); }
};

class MediaDisplayComponent : public Component,
                              public ChangeListener,
                              public ChangeBroadcaster,
                              public FileDragAndDropTarget,
                              public DragAndDropContainer,
                              private Timer,
                              private ScrollBar::Listener
{
public:
    MediaDisplayComponent();
    MediaDisplayComponent(String name, bool req = true, DisplayMode mode = DisplayMode::Hybrid);
    ~MediaDisplayComponent() override;

    virtual StringArray getInstanceExtensions() = 0;

    void paint(Graphics& g) override;
    virtual void resized() override;
    void repositionLabels();

    void changeListenerCallback(ChangeBroadcaster*) override { repaint(); }

    void setTrackName(String name) { trackName = name; }
    String getTrackName() { return trackName; }

    bool isRequired() const { return required; }

    bool isInputTrack() { return (displayMode == 0) || isHybridTrack(); }
    bool isOutputTrack() { return (displayMode == 1) || isHybridTrack(); }
    bool isHybridTrack() { return displayMode == 2; }
    bool isThumbnailTrack() { return displayMode == 3; }
    //DisplayMode getDisplayMode() { return displayMode; }

    void setTrackId(Uuid id) { trackID = id; }
    Uuid getTrackId() { return trackID; }

    void resetDisplay(); // Reset all state and media
    void initializeDisplay(const URL& filePath); // Initialize new display
    void updateDisplay(const URL& filePath); // Add new file to existing display

    virtual void loadMediaFile(const URL& filePath) = 0;

    //

    URL getTargetFilePath() { return targetFilePath; }

    bool isFileLoaded() { return ! tempFilePaths.isEmpty(); }

    void addNewTempFile();

    URL getTempFilePath() { return tempFilePaths.getReference(currentTempFileIdx); }

    bool iteratePreviousTempFile();
    bool iterateNextTempFile();

    void clearFutureTempFiles();

    void overwriteTarget();

    bool isInterestedInFileDrag(const StringArray& /*files*/) override { return isInputTrack(); }

    void filesDropped(const StringArray& files, int /*x*/, int /*y*/) override;

    URL getDroppedFilePath() { return droppedFilePath; }

    bool isFileDropped() { return ! droppedFilePath.isEmpty(); }

    void openFileChooser();

    // Callback for the save button
    void saveCallback();

    void clearDroppedFile() { droppedFilePath = URL(); }

    virtual void setPlaybackPosition(double t) { transportSource.setPosition(t); }
    virtual double getPlaybackPosition() { return transportSource.getCurrentPosition(); }

    void mouseDown(const MouseEvent& e) override { mouseDrag(e); }
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;

    virtual bool isPlaying() { return transportSource.isPlaying(); }
    virtual void startPlaying() { transportSource.start(); }
    virtual void stopPlaying() { transportSource.stop(); }

    void start();
    void stop();

    virtual double getTotalLengthInSecs() = 0;
    virtual double getTimeAtOrigin() { return visibleRange.getStart(); }
    virtual float getPixelsPerSecond();

    virtual void updateVisibleRange(Range<double> r);

    String getMediaHandlerInstructions();
    void setMediaHandlerInstructions(String instructions);

    void addLabels(LabelList& labels);
    void clearLabels(int processingIdxCutoff = 0);

    void addLabelOverlay(LabelOverlayComponent* l);
    void removeLabelOverlay(LabelOverlayComponent* l);

    void addOverheadLabel(OverheadLabelComponent* l);
    void removeOverheadLabel(OverheadLabelComponent* l);

    int getNumOverheadLabels();

    std::function<void(const String&)> instructionBoxWriter;

protected:
    virtual bool shouldRenderLabel(const std::unique_ptr<OutputLabel>& /*label*/) const
    {
        return true;
    }

    void setNewTarget(URL filePath);

    void resetTransport();

    void horizontalMove(float deltaX);

    void horizontalZoom(float deltaZoom, float scrollPosX);

    void mouseWheelMove(const MouseEvent&, const MouseWheelDetails& wheel) override;

    // Media (audio or MIDI) content area
    Component mediaComponent;

    const int controlSpacing = 1;
    const int scrollBarSize = 8;

    const int textSpacing = 2;
    const int minFontSize = 10;
    const int labelHeight = 20;

    Range<double> visibleRange;

    OverheadPanel overheadPanel;
    ScrollBar horizontalScrollBar { false };

    String mediaHandlerInstructions;

    AudioFormatManager formatManager;
    AudioDeviceManager deviceManager;

    AudioSourcePlayer sourcePlayer;
    AudioTransportSource transportSource;

    std::unique_ptr<FileChooser> openFileBrowser;
    std::unique_ptr<FileChooser> saveFileBrowser;

    SharedResourcePointer<InstructionBox> instructionBox;
    SharedResourcePointer<StatusBox> statusBox;

private:
    void initializeButtons();

    virtual Component* getMediaComponent() { return this; }

    virtual float getMediaHeight() { return static_cast<float>(getMediaComponent()->getHeight()); }
    virtual float getMediaWidth() { return static_cast<float>(getMediaComponent()->getWidth()); }
    virtual float getVerticalControlsWidth() { return 0.0f; }

    virtual float getMediaXPos() { return 0.0f; }
    double mediaXToTime(const float x);
    float timeToMediaX(const double t);
    float mediaXToDisplayX(const float mX);
    virtual float mediaYToDisplayY(const float mY) { return mY; }

    virtual void resetMedia() = 0;
    void resetPaths();
    void resetScrollBar();

    virtual void postLoadActions(const URL& filePath) = 0;

    int correctToBounds(float x, float width);

    void updateCursorPosition();

    void timerCallback() override;

    void scrollBarMoved(ScrollBar* scrollBarThatHasMoved, double scrollBarRangeStart) override;

    void mouseEnter(const MouseEvent& /*event*/) override;
    void mouseExit(const MouseEvent& /*event*/) override;

    // Flex for whole display
    FlexBox mainFlexBox;
    // Flex for header area (label and buttons)
    FlexBox headerFlexBox;
    // Flex for header buttons
    FlexBox buttonsFlexBox;
    // Flex for media / overhead panel (if any)
    FlexBox mediaAreaFlexBox;

    // Panel with labels / buttons
    Component headerComponent;
    // Media + overhead panel (if any)
    Component mediaAreaContainer;

    // Header sub-components
    Label trackNameLabel;
    MultiButton playStopButton;
    MultiButton::Mode playButtonInfo;
    MultiButton::Mode stopButtonInfo;
    MultiButton chooseFileButton;
    MultiButton::Mode chooseButtonInfo;
    MultiButton saveFileButton;
    MultiButton::Mode saveButtonActiveInfo;
    MultiButton::Mode saveButtonInactiveInfo;

    Uuid trackID;
    String trackName;
    const bool required = true;
    const DisplayMode displayMode;

    URL targetFilePath;
    URL droppedFilePath;

    int currentTempFileIdx;
    Array<URL> tempFilePaths;

    const float cursorWidth = 1.5f;
    DrawableRectangle currentPositionMarker;

    double currentHorizontalZoomFactor;

    OwnedArray<LabelOverlayComponent> labelOverlays;
    OwnedArray<OverheadLabelComponent> overheadLabels;
};
