#pragma once

#include "juce_core/juce_core.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <juce_audio_utils/juce_audio_utils.h>

#include "../utils.h"
#include "OutputLabelComponent.h"

using namespace juce;

class OverheadPanel : public Component
{
public:
    void paint(Graphics& g) override
    {
        g.fillAll(Colours::darkgrey.darker());
    }
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
    ~MediaDisplayComponent();

    virtual StringArray getInstanceExtensions() = 0;

    void paint(Graphics& g) override;
    virtual void resized() override;
    virtual void repositionOverheadPanel();
    Rectangle<int> getContentBounds();
    virtual void repositionContent() {};
    virtual void repositionScrollBar();

    virtual Component* getMediaComponent() { return this; }
    virtual float getMediaXPos() { return 0.0f; }
    float getMediaHeight() { return getMediaComponent()->getHeight(); }
    float getMediaWidth() { return getMediaComponent()->getWidth(); }

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

    void addLabels(LabelList& labels);
    void clearLabels(int processingIdxCutoff = 0);

    void addLabelOverlay(LabelOverlayComponent l);
    void removeLabelOverlay(LabelOverlayComponent* l);

    void addOverheadLabel(OverheadLabelComponent l);
    void removeOverheadLabel(OverheadLabelComponent* l);

    int getNumOverheadLabels();

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

private:
    void resetPaths();

    virtual void resetDisplay() = 0;

    virtual void postLoadActions(const URL& filePath) = 0;

    int correctToBounds(float x, float width);

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

    Array<LabelOverlayComponent*> labelOverlays;
    Array<OverheadLabelComponent*> overheadLabels;
};
