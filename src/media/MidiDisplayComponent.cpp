#include "MidiDisplayComponent.h"

MidiDisplayComponent::MidiDisplayComponent()
    : MediaDisplayComponent("Midi Track")
{
}

MidiDisplayComponent::MidiDisplayComponent(String trackName)
    : MediaDisplayComponent(trackName)
{
    pianoRoll.addMouseListener(this, true);
    pianoRoll.addChangeListener(this);
    addAndMakeVisible(pianoRoll);

    mediaHandlerInstructions =
        "MIDI pianoroll.\nClick and drag to start playback from any point in the pianoroll\nVertical or Horizontal scroll to move.\nCmd+scroll to zoom in both axis.";
}

MidiDisplayComponent::~MidiDisplayComponent()
{
    resetTransport();

    pianoRoll.removeMouseListener(this);
    pianoRoll.removeChangeListener(this);
}

StringArray MidiDisplayComponent::getSupportedExtensions()
{
    StringArray extensions;

    extensions.add(".mid");
    extensions.add(".midi");

    return extensions;
}

// void MidiDisplayComponent::repositionContent() { pianoRoll.setBounds(getContentBounds()); }

void MidiDisplayComponent::repositionScrollBar()
{
    Rectangle<int> scrollBarArea =
        getLocalBounds().removeFromBottom(scrollBarSize + 2 * controlSpacing);
    scrollBarArea = scrollBarArea.removeFromRight(
        scrollBarArea.getWidth() - pianoRoll.getKeyboardWidth() - pianoRoll.getPianoRollSpacing());
    scrollBarArea =
        scrollBarArea.removeFromLeft(scrollBarArea.getWidth() - 2 * pianoRoll.getScrollBarSize()
                                     - 4 * pianoRoll.getScrollBarSpacing());

    horizontalScrollBar.setBounds(scrollBarArea.reduced(controlSpacing));
}

void MidiDisplayComponent::loadMediaFile(const URL& filePath)
{
    // Create the local file this URL points to
    File file = filePath.getLocalFile();

    std::unique_ptr<juce::FileInputStream> fileStream(file.createInputStream());

    // Read the MIDI file from the File object
    MidiFile midiFile;

    if (! midiFile.readFrom(*fileStream))
    {
        DBG("Failed to read MIDI data from file.");
    }

    midiFile.convertTimestampTicksToSeconds();

    totalLengthInSecs = midiFile.getLastTimestamp();

    DBG("Total duration of MIDI file " << totalLengthInSecs << " seconds.");

    pianoRoll.resizeNoteGrid(totalLengthInSecs);

    juce::MidiMessageSequence allTracks;

    for (int trackIdx = 0; trackIdx < midiFile.getNumTracks(); ++trackIdx)
    {
        const juce::MidiMessageSequence* constTrack = midiFile.getTrack(trackIdx);

        if (constTrack != nullptr)
        {
            allTracks.addSequence(*constTrack, 0.0);
            allTracks.updateMatchedPairs();
        }
    }

    // A vector to keep all the midi numbers
    // we'll use that find the median note
    std::vector<int> midiNumbers;

    for (int eventIdx = 0; eventIdx < allTracks.getNumEvents(); ++eventIdx)
    {
        const auto midiEvent = allTracks.getEventPointer(eventIdx);
        const auto& midiMessage = midiEvent->message;

        double startTime = midiEvent->message.getTimeStamp();

        DBG("Event " << eventIdx << " at " << startTime << ": " << midiMessage.getDescription());

        if (midiMessage.isNoteOn())
        {
            int noteNumber = midiMessage.getNoteNumber();
            int velocity = midiMessage.getVelocity();
            int noteChannel = midiMessage.getChannel();

            midiNumbers.push_back(noteNumber);
            double duration = 0;

            for (int offIdx = eventIdx + 1; offIdx < allTracks.getNumEvents(); ++offIdx)
            {
                const auto offEvent = allTracks.getEventPointer(offIdx);

                // Find the matching note off event
                if (offEvent->message.isNoteOff() && offEvent->message.getNoteNumber() == noteNumber
                    && offEvent->message.getChannel() == noteChannel)
                {
                    duration =
                        (offEvent->message.getTimeStamp() - midiEvent->message.getTimeStamp());
                    break;
                }
            }

            // Create a component for each for each note
            MidiNoteComponent n = MidiNoteComponent(noteNumber, velocity, startTime, duration);
            pianoRoll.insertNote(n);
        }
    }

    // Find the median note
    std::sort(midiNumbers.begin(), midiNumbers.end());
    medianMidi = midiNumbers[midiNumbers.size() / 2];
    // Find std
    int sum = std::accumulate(midiNumbers.begin(), midiNumbers.end(), 0.0);
    float mean = sum / midiNumbers.size();
    float sq_sum =
        std::inner_product(midiNumbers.begin(), midiNumbers.end(), midiNumbers.begin(), 0.0);
    stdDevMidi = std::sqrt(sq_sum / midiNumbers.size() - mean * mean);

    synthAudioSource.useSequence(allTracks);
    transportSource.setSource(&synthAudioSource);
}

void MidiDisplayComponent::startPlaying()
{
    synthAudioSource.resetNotes();
    transportSource.start();
}

void MidiDisplayComponent::updateVisibleRange(Range<double> newRange)
{
    pianoRoll.updateVisibleMediaRange(newRange);
    MediaDisplayComponent::updateVisibleRange(newRange);
}

void MidiDisplayComponent::addLabels(LabelList& labels)
{
    MediaDisplayComponent::addLabels(labels);

    for (const auto& l : labels)
    {
        if (auto midiLabel = dynamic_cast<MidiLabel*>(l.get()))
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

            Colour clr = Colours::purple.withAlpha(0.8f);

            if ((l->color).has_value())
            {
                clr = Colour((l->color).value());
            }

            if ((midiLabel->pitch).has_value())
            {
                float p = (midiLabel->pitch).value();

                float y = LabelOverlayComponent::pitchToRelativeY(p);

                addLabelOverlay(
                    LabelOverlayComponent((double) l->t, lbl, y, (double) dur, dsc, clr));
            }
            else
            {
                // TODO - OverheadLabelComponent((double) l->t, lbl, (double) dur, dsc, clr);
            }
        }
    }
}

void MidiDisplayComponent::resized()
{
    MediaDisplayComponent::resized();
    pianoRoll.setBounds(mediaComponent.getBounds());
}
void MidiDisplayComponent::resetDisplay()
{
    MediaDisplayComponent::resetTransport();

    pianoRoll.resetNotes();
    pianoRoll.resizeNoteGrid(0.0);
}

void MidiDisplayComponent::postLoadActions(const URL& filePath)
{
    // Auto-center the pianoRoll viewbox on the median note
    pianoRoll.autoCenterViewBox(medianMidi, stdDevMidi);
}

void MidiDisplayComponent::verticalMove(float deltaY)
{
    pianoRoll.verticalMouseWheelMoveEvent(deltaY);
}

void MidiDisplayComponent::verticalZoom(float deltaZoom, float scrollPosY)
{
    pianoRoll.verticalMouseWheelZoomEvent(deltaZoom, scrollPosY);
}

void MidiDisplayComponent::mouseWheelMove(const MouseEvent& evt, const MouseWheelDetails& wheel)
{
    // DBG("Mouse wheel moved: deltaX=" << wheel.deltaX << ", deltaY=" << wheel.deltaY << ", scrollPos:" << evt.position.getX());

    if (getTotalLengthInSecs() > 0.0)
    {
        bool isCmdPressed = evt.mods.isCommandDown(); // Command key
        bool isShiftPressed = evt.mods.isShiftDown(); // Shift key
        bool isCtrlPressed = evt.mods.isCtrlDown(); // Control key
#if (JUCE_MAC)
        bool zoomMod = isCmdPressed;
#else
        bool zoomMod = isCtrlPressed;
#endif

        auto totalLength = visibleRange.getLength();
        auto visibleStart = visibleRange.getStart();
        auto scrollTime = mediaXToTime(evt.position.getX());
        DBG("Visible range: (" << visibleStart << ", " << visibleStart + totalLength
                               << ") Scrolled at time: " << scrollTime);

        if (zoomMod)
        {
            if (std::abs(wheel.deltaX) > 1 * std::abs(wheel.deltaY))
            {
                // Horizontal scroll when using 2-finger swipe in macbook trackpad
                horizontalZoom(wheel.deltaX, (float) scrollTime);
            }
            else if (std::abs(wheel.deltaY) > 1 * std::abs(wheel.deltaX))
            {
                // Vertical scroll
                verticalZoom(wheel.deltaY, (float) scrollTime);
            }
            else
            {
                // Do nothing
            }
        }
        else
        {
            if (std::abs(wheel.deltaX) > 0)
            {
                // Horizontal scroll when using 2-finger swipe in macbook trackpad
                horizontalMove(wheel.deltaX);
            }
            if (std::abs(wheel.deltaY) > 0)
            {
                verticalMove(-wheel.deltaY);
            }
        }
        repaint();
    }
}