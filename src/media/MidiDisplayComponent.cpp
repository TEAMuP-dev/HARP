#include "MidiDisplayComponent.h"

MidiDisplayComponent::MidiDisplayComponent() : MediaDisplayComponent("Midi Track") {}

MidiDisplayComponent::MidiDisplayComponent(String trackName, bool required)
    : MediaDisplayComponent(trackName, required)
{
    mediaComponent.addAndMakeVisible(pianoRoll);
    pianoRoll.addMouseListener(this, true);
    pianoRoll.addChangeListener(this);

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

void MidiDisplayComponent::loadMediaFile(const URL& filePath)
{
    DBG("Loading MIDI filePath: " << filePath.toString(true));
    // Create the local file this URL points to
    File file = filePath.getLocalFile();
    DBG("Loading MIDI file: " << file.getFullPathName());
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
            MidiNoteComponent n = MidiNoteComponent(static_cast<unsigned char>(noteNumber),
                                                    static_cast<unsigned char>(velocity),
                                                    startTime,
                                                    duration);
            pianoRoll.insertNote(n);
        }
    }

    // Find the median note
    std::sort(midiNumbers.begin(), midiNumbers.end());
    medianMidi = midiNumbers[midiNumbers.size() / 2];
    // Find std
    float sum = std::accumulate(midiNumbers.begin(), midiNumbers.end(), 0.0f);
    float mean = sum / static_cast<float>(midiNumbers.size());
    float sq_sum =
        std::inner_product(midiNumbers.begin(), midiNumbers.end(), midiNumbers.begin(), 0.0f);
    stdDevMidi = std::sqrt(sq_sum / static_cast<float>(midiNumbers.size()) - mean * mean);

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

void MidiDisplayComponent::resized()
{
    MediaDisplayComponent::resized();
    pianoRoll.setBounds(mediaComponent.getBounds().withY(0));
    horizontalScrollBar.setBounds(
        horizontalScrollBar.getBounds()
            .withTrimmedLeft(pianoRoll.getKeyboardWidth() + pianoRoll.getPianoRollSpacing())
            .withTrimmedRight(2 * pianoRoll.getScrollBarSize()
                              + 4 * pianoRoll.getScrollBarSpacing()));
}

void MidiDisplayComponent::resetDisplay()
{
    MediaDisplayComponent::resetTransport();

    pianoRoll.resetNotes();
    pianoRoll.resizeNoteGrid(0.0);
}

void MidiDisplayComponent::postLoadActions(const URL& /*filePath*/)
{
    // Auto-center the pianoRoll viewbox on the median note
    pianoRoll.autoCenterViewBox(medianMidi, stdDevMidi);
}

void MidiDisplayComponent::verticalMove(float deltaY)
{
    pianoRoll.verticalMouseWheelMoveEvent(deltaY);
}

void MidiDisplayComponent::verticalZoom(float deltaZoom)
{
    pianoRoll.verticalMouseWheelZoomEvent(deltaZoom);
}

void MidiDisplayComponent::mouseWheelMove(const MouseEvent& evt, const MouseWheelDetails& wheel)
{
    if (getTotalLengthInSecs() > 0.0)
    {
#if (JUCE_MAC)
        bool commandMod = evt.mods.isCommandDown();
#else
        bool commandMod = evt.mods.isCtrlDown();
#endif
        bool shiftMod = evt.mods.isShiftDown();

        if (commandMod)
        {
            if (std::abs(wheel.deltaY) > 2 * std::abs(wheel.deltaX))
            {
                if (shiftMod)
                {
                    verticalMove(-wheel.deltaY);
                }
                else
                {
                    verticalZoom(wheel.deltaY / 2);
                }
            }
            else
            {
                // Do nothing
            }
        }
        else
        {
            MediaDisplayComponent::mouseWheelMove(evt, wheel);
        }
        repaint();
    }
}
