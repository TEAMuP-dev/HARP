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

    void setTrackName(String name) { trackName = name; }
    String getTrackName() { return trackName; }

    bool isRequired() const { return required; }

    bool isInputTrack() { return (displayMode == 0) || isHybridTrack(); }
    bool isOutputTrack() { return (displayMode == 1) || isHybridTrack(); }
    bool isHybridTrack() { return displayMode == 2; }
    bool isThumbnailTrack() { return displayMode == 3; }

    void setDisplayID(Uuid id) { displayID = id; }
    Uuid getDisplayID() { return displayID; }

    String getMediaInstructions();

    void resetDisplay(); // Reset all state and media
    void initializeDisplay(const URL& filePath); // Initialize new display
    void updateDisplay(const URL& filePath); // Add new file to existing display

    virtual void loadMediaFile(const URL& filePath) = 0;

    URL getOriginalFilePath() { return originalFilePath; }

    void addNewTempFile(); // Create working temp file to modify instead of current or original
    bool iteratePreviousTempFile();
    bool iterateNextTempFile();

    bool isFileLoaded() { return ! tempFilePaths.isEmpty(); }
    URL getTempFilePath() { return tempFilePaths.getReference(currentTempFileIdx); }

    void clearFutureTempFiles(); // Prune temp files after currently selected index
    void overwriteOriginalFile(); // Necessary for seamless sample editing integration

    bool isInterestedInFileDrag(const StringArray& /*files*/) override { return isInputTrack(); }

    virtual double getTotalLengthInSecs() = 0;
    virtual double getTimeAtOrigin() { return visibleRange.getStart(); }
    virtual float getPixelsPerSecond();

    virtual void setPlaybackPosition(double t) { transportSource.setPosition(t); }
    virtual double getPlaybackPosition() { return transportSource.getCurrentPosition(); }

    void start();
    void stop();

    virtual bool isPlaying() { return transportSource.isPlaying(); }
    virtual void startPlaying() { transportSource.start(); }
    virtual void stopPlaying() { transportSource.stop(); }

    //

    void addLabels(LabelList& labels);
    void clearLabels(int processingIdxCutoff = 0);

    void addLabelOverlay(LabelOverlayComponent* l);
    void removeLabelOverlay(LabelOverlayComponent* l);

    void addOverheadLabel(OverheadLabelComponent* l);
    void removeOverheadLabel(OverheadLabelComponent* l);

    int getNumOverheadLabels();

protected:
    void resetTransport();

    virtual void updateVisibleRange(Range<double> r);

    virtual void mouseWheelMove(const MouseEvent&, const MouseWheelDetails& wheel) override;

    // Media (audio or MIDI) content area
    Component contentComponent;

    String mediaInstructions;

    //

    const int controlSpacing = 1;
    const int scrollBarSize = 8;

    const int textSpacing = 2;
    const int minFontSize = 10;
    const int labelHeight = 20;

    Range<double> visibleRange;

    OverheadPanel overheadPanel;
    ScrollBar horizontalScrollBar { false };

    AudioFormatManager formatManager;
    AudioDeviceManager deviceManager;

    AudioSourcePlayer sourcePlayer;
    AudioTransportSource transportSource;

private:
    void initializeButtons();

    virtual void visibleRangeCallback() { repaint(); }
    virtual void changeListenerCallback(ChangeBroadcaster*) override { repaint(); }

    virtual void resetMedia() = 0;
    void resetPaths();
    void resetScrollBar();
    void resetButtonState();

    void setOriginalFilePath(URL filePath);

    virtual void postLoadActions(const URL& filePath) = 0;

    void horizontalMove(double deltaT);
    void horizontalZoom(double deltaZoom, double scrollPosT);

    void filesDropped(const StringArray& files, int /*x*/, int /*y*/) override;

    void chooseFileCallback();
    void saveFileCallback();

    virtual Component* getMediaComponent() { return this; }

    virtual float getMediaHeight() { return static_cast<float>(getMediaComponent()->getHeight()); }
    virtual float getMediaWidth() { return static_cast<float>(getMediaComponent()->getWidth()); }
    virtual float getVerticalControlsWidth() { return 0.0f; }

    virtual float getMediaXPos() { return 0.0f; }
    double mediaXToTime(const float x);
    float timeToMediaX(const double t);
    float mediaXToDisplayX(const float mX);
    virtual float mediaYToDisplayY(const float mY) { return mY; }

    void scrollBarMoved(ScrollBar* scrollBarThatHasMoved, double scrollBarRangeStart) override;

    void updateCursorPosition();

    void mouseEnter(const MouseEvent& /*e*/) override;
    void mouseExit(const MouseEvent& /*e*/) override;

    void mouseDown(const MouseEvent& e) override { mouseDrag(e); }
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;

    virtual bool shouldRenderLabel(const std::unique_ptr<OutputLabel>& /*l*/) const { return true; }

    //

    void timerCallback() override;

    int correctToBounds(float x, float width);

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
    MultiButton::Mode playButtonActiveInfo;
    MultiButton::Mode playButtonInactiveInfo;
    MultiButton::Mode stopButtonInfo;
    MultiButton chooseFileButton;
    MultiButton::Mode chooseButtonInfo;
    MultiButton saveFileButton;
    MultiButton::Mode saveButtonActiveInfo;
    MultiButton::Mode saveButtonInactiveInfo;

    Uuid displayID;
    String trackName;
    const bool required = true;
    const DisplayMode displayMode;

    URL originalFilePath;
    int currentTempFileIdx;
    Array<URL> tempFilePaths;

    std::unique_ptr<FileChooser> chooseFileBrowser;
    std::unique_ptr<FileChooser> saveFileBrowser;

    double horizontalZoomFactor;

    const float cursorWidth = 1.5f;
    DrawableRectangle currentPositionCursor;

    OwnedArray<LabelOverlayComponent> labelOverlays;
    OwnedArray<OverheadLabelComponent> overheadLabels;

    SharedResourcePointer<InstructionBox> instructionBox;
    SharedResourcePointer<StatusBox> statusBox;
};
