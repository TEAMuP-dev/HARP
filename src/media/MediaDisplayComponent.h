#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include "juce_core/juce_core.h"

#include "OutputLabelComponent.h"
#include "../utils.h"

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
    ~MediaDisplayComponent();

    virtual StringArray getInstanceExtensions() = 0;

    void paint(Graphics& g) override;
    virtual void drawMainArea(Graphics& g, Rectangle<int>& a) = 0;
    virtual void resized() override;

    virtual float getMediaStartPos() { return 0.0f; }
    virtual float getMediaWidth() { return 0.0f; }

    void repositionOverheadLabels();
    void repositionLabelOverlays();

    void changeListenerCallback(ChangeBroadcaster*) override { repaint(); }

    virtual void loadMediaFile(const URL& filePath) = 0;

    void resetMedia();

    void setupDisplay(const URL& filePath);
    void updateDisplay(const URL& filePath);

    URL getTargetFilePath() { return targetFilePath; }

    bool isFileLoaded() { return !tempFilePaths.isEmpty(); }

    void addNewTempFile();

    URL getTempFilePath() { return tempFilePaths.getReference(currentTempFileIdx); }

    bool iteratePreviousTempFile();
    bool iterateNextTempFile();

    void clearFutureTempFiles();

    void overwriteTarget();

    bool isInterestedInFileDrag(const StringArray& /*files*/) override { return true; }

    void filesDropped(const StringArray& files, int /*x*/, int /*y*/) override;

    URL getDroppedFilePath() { return droppedFilePath; }

    bool isFileDropped() { return !droppedFilePath.isEmpty(); }

    void clearDroppedFile() { droppedFilePath = URL(); }

    virtual void setPlaybackPosition(double t) = 0;
    virtual double getPlaybackPosition() = 0;

    void mouseDown(const MouseEvent& e) override { mouseDrag(e); }
    void mouseDrag(const MouseEvent& e) override;
    void mouseUp(const MouseEvent& e) override;

    virtual bool isPlaying() = 0;
    virtual void startPlaying() = 0;
    virtual void stopPlaying() = 0;

    void start();
    void stop();

    virtual double getTotalLengthInSecs() = 0;

    virtual void updateVisibleRange(Range<double> r);

    String getMediaHandlerInstructions() { return mediaHandlerInstructions; }

    virtual void addLabels(LabelList& labels);

    void addLabelOverlay(LabelOverlayComponent l);
    void addOverheadLabel(OverheadLabelComponent l);

    void removeOutputLabel(OutputLabelComponent* l);
    void clearLabels();

protected:

    void setNewTarget(URL filePath);

    double xToTime(const float x);
    float timeToX(const double t);
    bool withinMediaBounds(const float x) { return x >= getMediaStartPos() && x <= getMediaStartPos() + getMediaWidth(); }

    int scrollBarSize = 10;
    int labelHeight = 20;
    int spacing = 2;

    Range<double> visibleRange;

    ScrollBar horizontalScrollBar{ false };

    String mediaHandlerInstructions;

private:

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

    Array<LabelOverlayComponent*> labelOverlays;
    Array<OverheadLabelComponent*> oveheadLabels;
};
