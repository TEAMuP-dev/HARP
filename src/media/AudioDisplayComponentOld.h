
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>


using namespace juce;

inline std::unique_ptr<InputSource> makeInputSource (const URL& url)
{
    if (const auto doc = AndroidDocument::fromDocument (url))
        return std::make_unique<AndroidDocumentInputSource> (doc);

   #if ! JUCE_IOS
    if (url.isLocalFile())
        return std::make_unique<FileInputSource> (url.getLocalFile());
   #endif

    return std::make_unique<URLInputSource> (url);
}


class ThumbnailComp  : public Component,
                           public ChangeListener,
                           public FileDragAndDropTarget,
                           public ChangeBroadcaster,
                           private ScrollBar::Listener,
                           private Timer
{
public:

    ThumbnailComp (AudioFormatManager& formatManager,
                       AudioTransportSource& source,
                       Slider& slider)
        : transportSource (source),
          zoomSlider (slider),
          thumbnail (512, formatManager, thumbnailCache)
    {
        thumbnail.addChangeListener (this);

        addAndMakeVisible (scrollbar);
        scrollbar.setRangeLimits (visibleRange);
        scrollbar.setAutoHide (false);
        scrollbar.addListener (this);

        currentPositionMarker.setFill (Colours::white.withAlpha (0.85f));
        addAndMakeVisible (currentPositionMarker);
    }

    ~ThumbnailComp() override
    {
        scrollbar.removeListener (this);
        thumbnail.removeChangeListener (this);
    }

    void setURL (const URL& url)
    {
        if (auto inputSource = makeInputSource (url))
        {
            thumbnailCache.clear();
            thumbnail.setSource (inputSource.release());

            Range<double> newRange (0.0, thumbnail.getTotalLength());
            scrollbar.setRangeLimits (newRange);
            setRange (newRange);

            startTimerHz (40);
        }
    }

    URL getLastDroppedFile() const noexcept { return lastFileDropped; }

    void setZoomFactor (double amount)
    {
        if (thumbnail.getTotalLength() > 0)
        {
            auto newScale = jmax (0.001, thumbnail.getTotalLength() * (1.0 - jlimit (0.0, 0.99, amount)));
            auto timeAtCentre = xToTime ((float) getWidth() / 2.0f);

            setRange ({ timeAtCentre - newScale * 0.5, timeAtCentre + newScale * 0.5 });
        }
    }

    void setRange (Range<double> newRange)
    {
        visibleRange = newRange;
        scrollbar.setCurrentRange (visibleRange);
        updateCursorPosition();
        repaint();
    }

    // void setFollowsTransport (bool shouldFollow)
    // {
    //     isFollowingTransport = shouldFollow;
    // }

    void paint (Graphics& g) override
    {
        g.fillAll (Colours::darkgrey);
        g.setColour (Colours::lightblue);

        if (thumbnail.getTotalLength() > 0.0)
        {
            auto thumbArea = getLocalBounds();

            thumbArea.removeFromBottom (scrollbar.getHeight() + 4);
            thumbnail.drawChannels (g, thumbArea.reduced (2),
                                    visibleRange.getStart(), visibleRange.getEnd(), 1.0f);
        }
        else
        {
            g.setFont (14.0f);
            g.drawFittedText ("(No audio file selected)", getLocalBounds(), Justification::centred, 2);
        }
    }

    void resized() override
    {
        scrollbar.setBounds (getLocalBounds().removeFromBottom (14).reduced (2));
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        // this method is called by the thumbnail when it has changed, so we should repaint it..
        repaint();
    }

    bool isInterestedInFileDrag (const StringArray& /*files*/) override
    {
        return true;
    }

    void mouseDown (const MouseEvent& e) override
    {
        mouseDrag (e);
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (canMoveTransport())
            transportSource.setPosition (jmax (0.0, xToTime ((float) e.x)));
            lastActionType = TransportMoved;
    }

    void mouseUp (const MouseEvent&) override
    {
        if (lastActionType == TransportMoved) {
            // transportSource.start();
            lastActionType = TransportStarted;
            sendChangeMessage();
        }
        
    }

    
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel) override
    {
        // DBG("Mouse wheel moved: deltaX=" << wheel.deltaX << ", deltaY=" << wheel.deltaY);
        if (thumbnail.getTotalLength() > 0.0)
        {
            if (std::abs(wheel.deltaX) > 2 * std::abs(wheel.deltaY)) {
                auto newStart = visibleRange.getStart() - wheel.deltaX * (visibleRange.getLength()) / 10.0;
                newStart = jlimit(0.0, jmax(0.0, thumbnail.getTotalLength() - visibleRange.getLength()), newStart);

                if (canMoveTransport())
                    setRange({ newStart, newStart + visibleRange.getLength() });
            } else if (std::abs(wheel.deltaY) > 2 * std::abs(wheel.deltaX)) {
                if (wheel.deltaY != 0) {
                    zoomSlider.setValue(zoomSlider.getValue() - wheel.deltaY);
                }
            } else {
                // Do nothing
            }
            repaint();
        }
    }


private:
    AudioTransportSource& transportSource;
    Slider& zoomSlider;
    ScrollBar scrollbar  { false };

    AudioThumbnailCache thumbnailCache  { 5 };
    AudioThumbnail thumbnail;
    Range<double> visibleRange;
    bool isFollowingTransport = true;
    URL lastFileDropped;

    DrawableRectangle currentPositionMarker;

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
        return ! (isFollowingTransport && transportSource.isPlaying());
    }

    void scrollBarMoved (ScrollBar* scrollBarThatHasMoved, double newRangeStart) override
    {
        if (scrollBarThatHasMoved == &scrollbar)
            if (! (isFollowingTransport && transportSource.isPlaying()))
                setRange (visibleRange.movedToStartAt (newRangeStart));
    }

    void timerCallback() override
    {
        if (canMoveTransport())
            updateCursorPosition();
        else
            setRange (visibleRange.movedToStartAt (transportSource.getCurrentPosition() - (visibleRange.getLength() / 2.0)));
    }

    void updateCursorPosition()
    {
        currentPositionMarker.setVisible (transportSource.isPlaying() || isMouseButtonDown());

        currentPositionMarker.setRectangle (Rectangle<float> (timeToX (transportSource.getCurrentPosition()) - 0.75f, 0,
                                                              1.5f, (float) (getHeight() - scrollbar.getHeight())));
    }
};
