#pragma once

#include <juce_audio_utils/juce_audio_utils.h>


using namespace juce;

class AudioDisplayComponent : public Component,
                              public ChangeListener,
                              public FileDragAndDropTarget,
                              public ChangeBroadcaster,
                              private ScrollBar::Listener,
                              private Timer
{

public:
    AudioDisplayComponent(AudioFormatManager& formatManager,
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
    }

    ~AudioDisplayComponent() override
    {
        scrollbar.removeListener (this);
        thumbnail.removeChangeListener (this);
    }

    void setURL (const URL& filePath)
    {
        if (auto inputSource = std::make_unique<URLInputSource>(filePath))
        {
            thumbnailCache.clear();
            thumbnail.setSource (inputSource.release());

            Range<double> newRange (0.0, thumbnail.getTotalLength());
            scrollbar.setRangeLimits (newRange);
            setRange (newRange);

            startTimerHz (40);
        }
    }

    void setRange (Range<double> newRange)
    {
        visibleRange = newRange;
        scrollbar.setCurrentRange (visibleRange);
        updateCursorPosition();
        repaint();
    }

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
    Slider& zoomSlider;
    ScrollBar scrollbar  { false };

    AudioThumbnailCache thumbnailCache  { 5 };

    Range<double> visibleRange;
};
