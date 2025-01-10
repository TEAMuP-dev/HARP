#pragma once

#include "juce_core/juce_core.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <juce_audio_utils/juce_audio_utils.h>

#include "../utils.h"
#include "OutputLabelComponent.h"
#include "../gui/MultiButton.h"


using namespace juce;

class MediaDisplayComponent : public Component,
                              public ChangeListener,
                              public ChangeBroadcaster,
                              public FileDragAndDropTarget,
                              private Timer,
                              private ScrollBar::Listener
{
public:
    MediaDisplayComponent();
    MediaDisplayComponent(String trackName);
    ~MediaDisplayComponent();

    virtual StringArray getInstanceExtensions() = 0;

    void paint(Graphics& g) override;
    virtual void resized() override;
    Rectangle<int> getContentBounds();
    // virtual void repositionContent() {};
    virtual void repositionScrollBar();

    virtual Component* getMediaComponent() { return this; }
    virtual float getMediaXPos() { return 0.0f; }

    String getTrackName() { return trackName; }
    float getMediaHeight() { return getMediaComponent()->getHeight(); }
    float getMediaWidth() { return getMediaComponent()->getWidth(); }

    void repositionOverheadLabels();
    void repositionLabelOverlays();
    void repositionLabels();

    void changeListenerCallback(ChangeBroadcaster*) override;

    virtual void loadMediaFile(const URL& filePath) = 0;

    void resetMedia();

    void setupDisplay(const URL& filePath);
    void updateDisplay(const URL& filePath);

    URL getTargetFilePath() { return targetFilePath; }

    bool isFileLoaded() { return ! tempFilePaths.isEmpty(); }

    void addNewTempFile();

    URL getTempFilePath() { return tempFilePaths.getReference(currentTempFileIdx); }

    bool iteratePreviousTempFile();
    bool iterateNextTempFile();

    void clearFutureTempFiles();

    void overwriteTarget();

    bool isInterestedInFileDrag(const StringArray& /*files*/) override { return true; }

    void filesDropped(const StringArray& files, int /*x*/, int /*y*/) override;

    URL getDroppedFilePath() { return droppedFilePath; }

    bool isFileDropped() { return ! droppedFilePath.isEmpty(); }

    void openFileChooser();

    // Callback for the save button
    void saveCallback();

    bool displaysInput() {return ioMode == 0;}
    bool displaysOutput() {return ioMode == 1;}
    
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
    virtual double getTimeAtOrigin() { return 0.0; }
    virtual float getPixelsPerSecond();

    virtual void updateVisibleRange(Range<double> r);

    String getMediaHandlerInstructions();

    virtual void addLabels(LabelList& labels);

    void addLabelOverlay(LabelOverlayComponent l);
    void addOverheadLabel(OverheadLabelComponent l);

    void removeOutputLabel(OutputLabelComponent* l);
    void clearLabels();

protected:
    void setNewTarget(URL filePath);

    double mediaXToTime(const float x);
    float timeToMediaX(const double t);
    float mediaXToDisplayX(const float mX);

    void resetTransport();

    void horizontalMove(float deltaX);

    void horizontalZoom(float deltaZoom, float scrollPosX);

    const int controlSpacing = 1;
    const int scrollBarSize = 8;

    Range<double> visibleRange;

    ScrollBar horizontalScrollBar { false };

    String mediaHandlerInstructions;

    AudioFormatManager formatManager;
    AudioDeviceManager deviceManager;

    AudioSourcePlayer sourcePlayer;
    AudioTransportSource transportSource;

    std::unique_ptr<FileChooser> openFileBrowser;
    std::unique_ptr<FileChooser> saveFileBrowser;

    juce::SharedResourcePointer<InstructionBox> instructionBox;
    juce::SharedResourcePointer<StatusBox> statusBox;

    juce::FlexBox mainFlexBox;
    juce::FlexBox headerFlexBox;
    juce::FlexBox mediaFlexBox;
    // Track sub-components
    // Left panel containing track name and buttons
    juce::Component headerComponent;
    // Media (audio or MIDI) content area
    juce::Component mediaComponent;  
    // Header sub-components
    juce::Label trackNameLabel;
    MultiButton playStopButton;
    MultiButton::Mode playButtonInfo;
    MultiButton::Mode stopButtonInfo;
    MultiButton chooseFileButton;
    MultiButton::Mode chooseButtonInfo;
    MultiButton saveFileButton;
    MultiButton::Mode saveButtonActiveInfo;
    MultiButton::Mode saveButtonInactiveInfo;

private:

    void populateTrackHeader();

    void resetPaths();

    virtual void resetDisplay() = 0;

    virtual void postLoadActions(const URL& filePath) = 0;

    void updateCursorPosition();

    void timerCallback() override;

    void scrollBarMoved(ScrollBar* scrollBarThatHasMoved, double scrollBarRangeStart) override;

    void mouseWheelMove(const MouseEvent&, const MouseWheelDetails& wheel) override;

    URL targetFilePath;
    URL droppedFilePath;

    int currentTempFileIdx;
    Array<URL> tempFilePaths;

    const float cursorWidth = 1.5f;
    DrawableRectangle currentPositionMarker;

    double currentHorizontalZoomFactor;

    const int textSpacing = 2;
    const int minFontSize = 10;
    const int labelHeight = 20;

    bool ioMode = 0; // 0 input, 1 output

    Array<LabelOverlayComponent*> labelOverlays;
    Array<OverheadLabelComponent*> oveheadLabels;

    juce::String trackName;
};
