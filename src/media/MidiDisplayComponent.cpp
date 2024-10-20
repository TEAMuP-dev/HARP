#include "MidiDisplayComponent.h"


MidiDisplayComponent::MidiDisplayComponent()
{
    pianoRoll.addMouseListener(this, true);
    pianoRoll.addChangeListener(this);
    addAndMakeVisible(pianoRoll);

    mediaHandlerInstructions = "MIDI pianoroll.\nClick and drag to start playback from any point in the pianoroll\nVertical scroll to zoom in/out.\nHorizontal scroll to move the pianoroll.";
}

MidiDisplayComponent::~MidiDisplayComponent()
{
    pianoRoll.removeMouseListener(this);
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

    for (int trackIdx = 0; trackIdx < midiFile.getNumTracks(); ++trackIdx) {
        const juce::MidiMessageSequence* constTrack = midiFile.getTrack(trackIdx);

        if (constTrack != nullptr) {
            juce::MidiMessageSequence track(*constTrack);
            track.updateMatchedPairs();

            DBG("Track " << trackIdx << " has " << track.getNumEvents() << " events.");

            for (int eventIdx = 0; eventIdx < track.getNumEvents(); ++eventIdx) {
                const auto midiEvent = track.getEventPointer(eventIdx);
                const auto& midiMessage = midiEvent->message;

                double startTime = midiEvent->message.getTimeStamp();

                DBG("Event " << eventIdx << " at " << startTime << ": " << midiMessage.getDescription());

                if (midiMessage.isNoteOn()) {
                    int noteNumber = midiMessage.getNoteNumber();
                    int velocity = midiMessage.getVelocity();

                    double duration = 0;

                    for (int offIdx = eventIdx + 1; offIdx < track.getNumEvents(); ++offIdx) {
                        const auto offEvent = track.getEventPointer(offIdx);

                        // Find the matching note off event
                        if (offEvent->message.isNoteOff() && offEvent->message.getNoteNumber() == noteNumber) {
                            duration = (offEvent->message.getTimeStamp() - midiEvent->message.getTimeStamp());
                            break;
                        }
                    }

                    // Create a component for each for each note
                    MidiNoteComponent n = MidiNoteComponent(noteNumber, velocity, startTime, duration);
                    pianoRoll.insertNote(n);
                }
            }
        }
    }
}

void MidiDisplayComponent::setPlaybackPosition(double t)
{
    // TODO
}

double MidiDisplayComponent::getPlaybackPosition()
{
    // TODO
    return 0.0;
}

bool MidiDisplayComponent::isPlaying()
{
    // TODO
    return false;
}

void MidiDisplayComponent::startPlaying()
{
    // TODO
    AlertWindow::showMessageBoxAsync(
        AlertWindow::WarningIcon,
        "NotImplementedError",
        "MIDI playback has not yet been implemented."
    );
}

void MidiDisplayComponent::stopPlaying()
{
    // TODO
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
    pianoRoll.resetNotes();
    pianoRoll.resizeNoteGrid(0.0);
}

void MidiDisplayComponent::postLoadActions(const URL& filePath) {}
