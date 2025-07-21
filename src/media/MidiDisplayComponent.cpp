#include "MidiDisplayComponent.h"

MidiDisplayComponent::MidiDisplayComponent() : MediaDisplayComponent("Midi Track") {}

MidiDisplayComponent::MidiDisplayComponent(String name, bool req, DisplayMode mode)
    : MediaDisplayComponent(name, req, mode)
{
    // Need to reposition labels after vertical zoom or scroll
    pianoRoll.addChangeListener(this);
    // Support for drag and drop
    pianoRoll.addMouseListener(this, true);
    contentComponent.addAndMakeVisible(pianoRoll);

    mediaInstructions =
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

    if (! midiFile.readFrom(*fileStream))
    {
        DBG("MidiDisplayComponent::loadMediaFile: Error reading MIDI from file "
            << file.getFullPathName() << ".");
        // TODO - better error handing
        //jassertfalse;
        //return;
    }

    midiFile.convertTimestampTicksToSeconds();

    totalLengthInSecs = midiFile.getLastTimestamp();

    //DBG("Total duration of MIDI file " << totalLengthInSecs << " seconds.");

    pianoRoll.resizeNoteGrid(totalLengthInSecs);

    MidiMessageSequence allTracks;

    for (int trackIdx = 0; trackIdx < midiFile.getNumTracks(); ++trackIdx)
    {
        const MidiMessageSequence* constTrack = midiFile.getTrack(trackIdx);

        if (constTrack != nullptr)
        {
            allTracks.addSequence(*constTrack, 0.0);
            allTracks.updateMatchedPairs();
        }
    }

    // Keep track of note MIDI numbers
    std::vector<int> midiNumbers;

    for (int eventIdx = 0; eventIdx < allTracks.getNumEvents(); ++eventIdx)
    {
        const auto midiEvent = allTracks.getEventPointer(eventIdx);
        const auto& midiMessage = midiEvent->message;

        double startTime = midiEvent->message.getTimeStamp();

        //DBG("Event " << eventIdx << " at " << startTime << ": " << midiMessage.getDescription());

        if (midiMessage.isNoteOn())
        {
            int noteChannel = midiMessage.getChannel();
            int noteNumber = midiMessage.getNoteNumber();
            int velocity = midiMessage.getVelocity();

            midiNumbers.push_back(noteNumber);

            double duration = 0;

            for (int offIdx = eventIdx + 1; offIdx < allTracks.getNumEvents(); ++offIdx)
            {
                const auto offEvent = allTracks.getEventPointer(offIdx);

                // Find matching note offset event
                if (offEvent->message.isNoteOff() && offEvent->message.getNoteNumber() == noteNumber
                    && offEvent->message.getChannel() == noteChannel)
                {
                    duration =
                        (offEvent->message.getTimeStamp() - midiEvent->message.getTimeStamp());
                    break;
                }
            }

            // Create component for each for each note and add to pianoroll
            MidiNote n = MidiNote(static_cast<unsigned char>(noteNumber),
                                  startTime,
                                  duration,
                                  static_cast<unsigned char>(velocity));
            pianoRoll.insertNote(n);
        }
    }

    // Compute median and standard deviation of MIDI numbers
    std::sort(midiNumbers.begin(), midiNumbers.end());

    int numNotes = midiNumbers.size();

    medianMidi = midiNumbers[numNotes / 2];

    if (! numNotes % 2)
    {
        medianMidi += midiNumbers[numNotes / 2 - 1];
        medianMidi /= 2;
    }

    float sum = std::accumulate(midiNumbers.begin(), midiNumbers.end(), 0.0f);
    float mean = sum / static_cast<float>(numNotes);
    float sq_sum =
        std::inner_product(midiNumbers.begin(), midiNumbers.end(), midiNumbers.begin(), 0.0f);

    stdDevMidi = std::sqrt(sq_sum / static_cast<float>(numNotes) - mean * mean);

    synthAudioSource.useSequence(allTracks);
    transportSource.setSource(&synthAudioSource);
}

void MidiDisplayComponent::startPlaying()
{
    synthAudioSource.resetNotes();
    transportSource.start();
}

void MidiDisplayComponent::resetMedia()
{
    resetTransport();

    pianoRoll.resetNotes();
    pianoRoll.resizeNoteGrid(0.0);

    totalLengthInSecs = 0.0;
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
    bool commandMod = evt.mods.isCommandDown();
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
    else
    {
        if (getTotalLengthInSecs() > 0.0)
        {
            MediaDisplayComponent::mouseWheelMove(evt, wheel);
        }
    }
}
