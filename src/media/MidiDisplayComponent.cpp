#include "MidiDisplayComponent.h"


MidiDisplayComponent::MidiDisplayComponent()
{
    thread.startThread(Thread::Priority::normal);

    formatManager.registerBasicFormats();

    deviceManager.initialise(0, 2, nullptr, true, {}, nullptr);
    deviceManager.addAudioCallback(&sourcePlayer);

    sourcePlayer.setSource(&transportSource);
    transportSource.setSource(&synthAudioSource);

    pianoRoll.addMouseListener(this, true);
    pianoRoll.addChangeListener(this);
    addAndMakeVisible(pianoRoll);

    mediaHandlerInstructions = "MIDI pianoroll.\nClick and drag to start playback from any point in the pianoroll\nVertical scroll to zoom in/out.\nHorizontal scroll to move the pianoroll.";
}

MidiDisplayComponent::~MidiDisplayComponent()
{
    deviceManager.removeAudioCallback(&sourcePlayer);

    transportSource.setSource(nullptr);
    sourcePlayer.setSource(nullptr);

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

void MidiDisplayComponent::repositionContent()
{
    pianoRoll.setBounds(getContentBounds());
}

void MidiDisplayComponent::repositionScrollBar()
{
    Rectangle<int> scrollBarArea = getLocalBounds().removeFromBottom(scrollBarSize + 2 * controlSpacing);
    scrollBarArea = scrollBarArea.removeFromRight(scrollBarArea.getWidth() - pianoRoll.getKeyboardWidth() - pianoRoll.getPianoRollSpacing());
    scrollBarArea = scrollBarArea.removeFromLeft(scrollBarArea.getWidth() - 2 * pianoRoll.getScrollBarSize() - 4 * pianoRoll.getScrollBarSpacing());

    horizontalScrollBar.setBounds(scrollBarArea.reduced(controlSpacing));
}

void MidiDisplayComponent::loadMediaFile(const URL& filePath)
{
    // Create the local file this URL points to
    File file = filePath.getLocalFile();

    std::unique_ptr<juce::FileInputStream> fileStream(file.createInputStream());

    // Read the MIDI file from the File object
    MidiFile midiFile;

    if (!midiFile.readFrom(*fileStream)) {
        DBG("Failed to read MIDI data from file.");
    }

    midiFile.convertTimestampTicksToSeconds();

    totalLengthInSecs = midiFile.getLastTimestamp();

    DBG("Total duration of MIDI file " << totalLengthInSecs << " seconds.");

    pianoRoll.resizeNoteGrid(totalLengthInSecs);

    juce::MidiMessageSequence allTracks;

    for (int trackIdx = 0; trackIdx < midiFile.getNumTracks(); ++trackIdx) {
        const juce::MidiMessageSequence* constTrack = midiFile.getTrack(trackIdx);

        if (constTrack != nullptr) {
            allTracks.addSequence(*constTrack, 0.0);
            allTracks.updateMatchedPairs();
        }
    }

    for (int eventIdx = 0; eventIdx < allTracks.getNumEvents(); ++eventIdx) {
        const auto midiEvent = allTracks.getEventPointer(eventIdx);
        const auto& midiMessage = midiEvent->message;

        double startTime = midiEvent->message.getTimeStamp();

        DBG("Event " << eventIdx << " at " << startTime << ": " << midiMessage.getDescription());

        if (midiMessage.isNoteOn()) {
            int noteNumber = midiMessage.getNoteNumber();
            int velocity = midiMessage.getVelocity();
            int noteChannel = midiMessage.getChannel();

            double duration = 0;

            for (int offIdx = eventIdx + 1; offIdx < allTracks.getNumEvents(); ++offIdx) {
                const auto offEvent = allTracks.getEventPointer(offIdx);

                // Find the matching note off event
                if (offEvent->message.isNoteOff() && offEvent->message.getNoteNumber() == noteNumber && offEvent->message.getChannel() == noteChannel) {
                    duration = (offEvent->message.getTimeStamp() - midiEvent->message.getTimeStamp());
                    break;
                }
            }

            // Create a component for each for each note
            MidiNoteComponent n = MidiNoteComponent(noteNumber, velocity, startTime, duration);
            pianoRoll.insertNote(n);
        }
    }

    synthAudioSource.useSequence(allTracks);
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

    for (const auto& l : labels) {
        if (auto midiLabel = dynamic_cast<MidiLabel*>(l.get())) {
            String lbl = l->label;
            String dsc = l->description;

            if (dsc.isEmpty()) {
                dsc = lbl;
            }

            float dur = 0.0f;

            if ((l->duration).has_value()) {
                dur = (l->duration).value();
            }

            if ((midiLabel->pitch).has_value()) {
                float p = (midiLabel->pitch).value();

                float y = LabelOverlayComponent::pitchToRelativeY(p);

                addLabelOverlay(LabelOverlayComponent((double) l->t, lbl, y, (double) dur, dsc));
            } else {
                // TODO - OverheadLabelComponent((double) l->t, lbl, (double) dur, dsc);
            }
        }
    }
}

void MidiDisplayComponent::resetDisplay()
{
    transportSource.stop();
    transportSource.setSource(nullptr);

    pianoRoll.resetNotes();
    pianoRoll.resizeNoteGrid(0.0);
}

void MidiDisplayComponent::postLoadActions(const URL& filePath) {}
