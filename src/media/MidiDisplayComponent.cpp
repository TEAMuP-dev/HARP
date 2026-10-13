#include "MidiDisplayComponent.h"

MidiDisplayComponent::MidiDisplayComponent() : MediaDisplayComponent("Midi Track") {}

MidiDisplayComponent::MidiDisplayComponent(String name, bool req, bool fromDAW, DisplayMode mode)
    : MediaDisplayComponent(name, req, fromDAW, mode)
{
    // Need to reposition labels after vertical zoom or scroll
    pianoRoll.addChangeListener(this);
    // Support for drag and drop
    pianoRoll.addMouseListener(this, true);
    contentComponent.addAndMakeVisible(pianoRoll);

#if JUCE_MAC
    const String pitchModifier = "Cmd";
#else
    const String pitchModifier = "Ctrl";
#endif

    mediaInstructions = "MIDI piano roll.\nClick anywhere to play from that point.\n"
                        "Scroll to zoom in time, or hold Shift while scrolling to pan.\nHold "
                        + pitchModifier + " while scrolling to zoom in pitch, or " + pitchModifier
                        + "+Shift to move through pitch.";
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

void MidiDisplayComponent::resized()
{
    MediaDisplayComponent::resized();

    pianoRoll.setBounds(contentComponent.getBounds().withY(0));
}

void MidiDisplayComponent::loadMediaFile(const URL& filePath)
{
    File file = filePath.getLocalFile();

    std::unique_ptr<FileInputStream> fileStream(file.createInputStream());

    MidiFile midiFile;

    if (fileStream == nullptr || ! midiFile.readFrom(*fileStream))
    {
        // Left empty, as a file without notes is
        DBG_AND_LOG("MidiDisplayComponent::loadMediaFile: Error reading MIDI from file "
                    << file.getFullPathName() << ".");

        return;
    }

    midiFile.convertTimestampTicksToSeconds();

    totalLengthInSecs = midiFile.getLastTimestamp();

    pianoRoll.resizeNoteGrid(totalLengthInSecs);

    MidiMessageSequence allTracks;

    for (int trackIdx = 0; trackIdx < midiFile.getNumTracks(); ++trackIdx)
    {
        if (const MidiMessageSequence* track = midiFile.getTrack(trackIdx))
        {
            allTracks.addSequence(*track, 0.0);
        }
    }

    // Pairs each note-on with the note-off that ends it
    allTracks.updateMatchedPairs();

    // Keep track of note MIDI numbers
    std::vector<int> midiNumbers;

    // Keep track of current instrument numbers by channel
    std::array<int, 16> instrumentNumbers;
    instrumentNumbers.fill(0);

    for (int eventIdx = 0; eventIdx < allTracks.getNumEvents(); ++eventIdx)
    {
        const auto midiEvent = allTracks.getEventPointer(eventIdx);
        const auto& midiMessage = midiEvent->message;

        // Only channel messages are used, which number their channels from 1 to 16
        const auto channelIdx = (size_t) (midiMessage.getChannel() - 1);

        if (midiMessage.isProgramChange())
        {
            // Update instrument number for current channel
            instrumentNumbers[channelIdx] = midiMessage.getProgramChangeNumber();
        }

        if (midiMessage.isNoteOn())
        {
            const int noteNumber = midiMessage.getNoteNumber();
            const double startTime = midiMessage.getTimeStamp();

            midiNumbers.push_back(noteNumber);

            // A note never released lasts no time at all
            const double duration =
                midiEvent->noteOffObject != nullptr
                    ? midiEvent->noteOffObject->message.getTimeStamp() - startTime
                    : 0.0;

            // Create component for each note and add to pianoroll
            pianoRoll.insertNote(MidiNote(static_cast<unsigned char>(noteNumber),
                                          startTime,
                                          duration,
                                          static_cast<unsigned char>(midiMessage.getVelocity()),
                                          static_cast<unsigned char>(instrumentNumbers[channelIdx]),
                                          midiMessage.getChannel() == 10)); // Drums
        }
    }

    if (! midiNumbers.empty())
    {
        // Compute median and standard deviation of MIDI numbers
        std::sort(midiNumbers.begin(), midiNumbers.end());

        const size_t numNotes = midiNumbers.size();

        medianMidi = numNotes % 2 == 0
                         ? (midiNumbers[numNotes / 2 - 1] + midiNumbers[numNotes / 2]) / 2
                         : midiNumbers[numNotes / 2];

        const float mean = std::accumulate(midiNumbers.begin(), midiNumbers.end(), 0.0f)
                           / static_cast<float>(numNotes);
        const float squareSum =
            std::inner_product(midiNumbers.begin(), midiNumbers.end(), midiNumbers.begin(), 0.0f);

        stdDevMidi = std::sqrt(jmax(0.0f, squareSum / static_cast<float>(numNotes) - mean * mean));
    }

    synthAudioSource.useSequence(allTracks);
    transportSource.setSource(&synthAudioSource);
}

void MidiDisplayComponent::resetMedia()
{
    resetTransport();

    pianoRoll.resetNotes();
    pianoRoll.resizeNoteGrid(0.0);

    totalLengthInSecs = 0.0;

    // Centered on middle C until notes say otherwise
    medianMidi = defaultMedianMidi;
    stdDevMidi = defaultStdDevMidi;
}

void MidiDisplayComponent::postLoadActions(const URL& /*filePath*/)
{
    // Auto-center pianoRoll vertically about median note
    pianoRoll.autoCenterViewBox(medianMidi, stdDevMidi);
}

float MidiDisplayComponent::getVerticalControlsWidth()
{
    return static_cast<float>(pianoRoll.getControlsWidth());
}

float MidiDisplayComponent::getMediaXPos()
{
    return static_cast<float>(pianoRoll.getKeyboardWidth() + pianoRoll.getPianoRollSpacing());
}

float MidiDisplayComponent::mediaYToDisplayY(const float mY)
{
    float visibleStartY =
        static_cast<float>(pianoRoll.getVisibleKeyRange().getStart()) * pianoRoll.getKeyHeight();

    float dY = mY - visibleStartY;

    return dY;
}

void MidiDisplayComponent::updateVisibleRange(Range<double> newRange)
{
    MediaDisplayComponent::updateVisibleRange(newRange);

    pianoRoll.updateVisibleMediaRange(newRange);
}

void MidiDisplayComponent::verticalMove(double deltaY)
{
    pianoRoll.verticalMouseWheelMoveEvent(deltaY);
}

void MidiDisplayComponent::verticalZoom(double deltaZoom)
{
    pianoRoll.verticalMouseWheelZoomEvent(deltaZoom);
}

void MidiDisplayComponent::mouseWheelMove(const MouseEvent& evt, const MouseWheelDetails& wheel)
{
#if (JUCE_MAC)
    bool commandMod = evt.mods.isCommandDown() || evt.mods.isCtrlDown();
#else
    bool commandMod = evt.mods.isCtrlDown();
#endif
    bool shiftMod = evt.mods.isShiftDown();

    if (! isThumbnailTrack() && commandMod)
    {
        if (std::abs(wheel.deltaY) > 2 * std::abs(wheel.deltaX))
        {
            if (shiftMod)
            {
                verticalMove(-static_cast<double>(wheel.deltaY));
            }
            else
            {
                verticalZoom(static_cast<double>(wheel.deltaY) / 2.0);
            }
        }
        else
        {
            // Do nothing
        }
    }
    else if (getTotalLengthInSecs() > 0.0)
    {
        MediaDisplayComponent::mouseWheelMove(evt, wheel);
    }
    else
    {
        // No timeline to move along, so let the panel scroll instead
        Component::mouseWheelMove(evt, wheel);
    }
}

void MidiDisplayComponent::setPlaybackPosition(double t)
{
    MediaDisplayComponent::setPlaybackPosition(t);

    // A seek during playback would otherwise hold the notes sounding at the old position
    synthAudioSource.resetNotes();
}

void MidiDisplayComponent::startPlaying()
{
    synthAudioSource.resetNotes();
    transportSource.start();
}
