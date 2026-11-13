/**
 * @file SynthAudioSource.h
 * @brief Handles MIDI playback for pianoroll.
 * @author nathanpruyne, cwitkowitz
 *
 * Note that this file was adapted from:
 * https://juce.com/tutorials/tutorial_synth_using_midi_input/
 */

#pragma once

#include <atomic>
#include <cmath>

#include <juce_audio_basics/juce_audio_basics.h>

using namespace juce;

struct SineWaveSound : public SynthesiserSound
{
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

struct SineWaveVoice : public SynthesiserVoice
{
    // Only the float overload is rendered, and the double one stays as JUCE defines it
    using SynthesiserVoice::renderNextBlock;

    bool canPlaySound(SynthesiserSound* sound) override
    {
        return dynamic_cast<SineWaveSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber,
                   float velocity,
                   SynthesiserSound*,
                   int /*currentPitchWheelPosition*/) override
    {
        const double cyclesPerSecond = MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        const double cyclesPerSample = cyclesPerSecond / getSampleRate();

        currentAngle = 0.0;
        angleDelta = cyclesPerSample * MathConstants<double>::twoPi;

        tailOff = 0.0;
        level = static_cast<double>(velocity) * maximumLevel;
    }

    void stopNote(float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            // Begins the release, unless the note is already in it
            if (exactlyEqual(tailOff, 0.0))
            {
                tailOff = 1.0;
            }
        }
        else
        {
            clearCurrentNote();

            angleDelta = 0.0;
        }
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(AudioSampleBuffer& outputBuffer, int startSample, int numSamples) override
    {
        // A voice with no note has no angle to advance
        if (exactlyEqual(angleDelta, 0.0))
        {
            return;
        }

        const bool isReleasing = tailOff > 0.0;

        while (--numSamples >= 0)
        {
            const double gain = isReleasing ? level * tailOff : level;
            const float currentSample = static_cast<float>(std::sin(currentAngle) * gain);

            for (int channel = outputBuffer.getNumChannels(); --channel >= 0;)
            {
                outputBuffer.addSample(channel, startSample, currentSample);
            }

            currentAngle += angleDelta;

            ++startSample;

            if (isReleasing)
            {
                tailOff *= tailOffDecay;

                if (tailOff <= tailOffFloor)
                {
                    clearCurrentNote();

                    angleDelta = 0.0;

                    break;
                }
            }
        }
    }

private:
    // Level of a note at full velocity, which leaves room for several to sound at once
    static constexpr double maximumLevel = 0.15;
    // Factor applied to a released note on each sample, and the level at which it ends
    static constexpr double tailOffDecay = 0.99;
    static constexpr double tailOffFloor = 0.005;

    double currentAngle = 0.0;
    double angleDelta = 0.0;
    double tailOff = 0.0;
    double level = 0.0;
};

/**
 * Plays a MIDI sequence, timed in seconds, through one sine voice per simultaneous note.
 *
 * The audio thread reads from this while the message thread moves the position, so the
 * position is atomic. A move is taken up at the start of the next block: every voice is
 * silenced, and the notes that are sounding at the new position start again, which would
 * otherwise stay silent until their next onset.
 */
class SynthAudioSource : public PositionableAudioSource
{
public:
    SynthAudioSource() { synth.addSound(new SineWaveSound()); }

    void prepareToPlay(int /*samplesPerBlockExpected*/, double newSampleRate) override
    {
        sampleRate = newSampleRate;

        synth.setCurrentPlaybackSampleRate(newSampleRate);
    }

    void releaseResources() override {}

    /**
     * Sets the sequence to play. The transport must not be reading from this source at the
     * time, i.e. this source must not be the one it has been given.
     */
    void useSequence(const MidiMessageSequence& midiSequence)
    {
        sequence = midiSequence;

        // Pairs each note-on with the note-off that ends it, on which a move relies
        sequence.updateMatchedPairs();

        int maxVoices = 0;
        int currentVoices = 0;

        // One voice for each of the most notes that ever sound at once
        for (const auto* midiEvent : sequence)
        {
            if (midiEvent->message.isNoteOn())
            {
                maxVoices = jmax(maxVoices, ++currentVoices);
            }
            else if (midiEvent->message.isNoteOff())
            {
                currentVoices = jmax(0, currentVoices - 1);
            }
        }

        synth.clearVoices();

        for (int i = 0; i < maxVoices; ++i)
        {
            synth.addVoice(new SineWaveVoice());
        }

        readPosition = 0;

        resetNotes();
    }

    void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) override
    {
        bufferToFill.clearActiveBufferRegion();

        // Read before the position, so that a move made between the two is taken up next block
        const bool wasMoved = needsResync.exchange(false);

        const int64 blockStart = readPosition.load();
        const int64 blockEnd = blockStart + bufferToFill.numSamples;

        MidiBuffer blockEvents;

        if (wasMoved)
        {
            synth.allNotesOff(0, false);

            nextEventIndex = 0;

            // Everything before the block is skipped, apart from the notes still sounding in it
            while (nextEventIndex < sequence.getNumEvents()
                   && getEventPosition(nextEventIndex) < blockStart)
            {
                const auto* midiEvent = sequence.getEventPointer(nextEventIndex);

                if (midiEvent->message.isNoteOn() && midiEvent->noteOffObject != nullptr
                    && secondsToSamples(midiEvent->noteOffObject->message.getTimeStamp())
                           > blockStart)
                {
                    blockEvents.addEvent(midiEvent->message, 0);
                }

                ++nextEventIndex;
            }
        }

        // The events of this block, each at its offset within the block
        while (nextEventIndex < sequence.getNumEvents()
               && getEventPosition(nextEventIndex) < blockEnd)
        {
            blockEvents.addEvent(sequence.getEventPointer(nextEventIndex)->message,
                                 static_cast<int>(getEventPosition(nextEventIndex) - blockStart));

            ++nextEventIndex;
        }

        synth.renderNextBlock(
            *bufferToFill.buffer, blockEvents, bufferToFill.startSample, bufferToFill.numSamples);

        // Advanced only if the position was not moved while this block was rendered
        int64 expectedPosition = blockStart;
        readPosition.compare_exchange_strong(expectedPosition, blockEnd);
    }

    void setNextReadPosition(int64 newPosition) override
    {
        readPosition = newPosition;

        resetNotes();
    }

    int64 getNextReadPosition() const override { return readPosition.load(); }

    // Up to the last MIDI message, which is likely a note off event
    int64 getTotalLength() const override { return secondsToSamples(sequence.getEndTime()); }

    void setLooping(bool /*shouldLoop*/) override {}
    bool isLooping() const override { return false; }

    /**
     * Silences every voice at the start of the next block, and starts the notes that are
     * sounding at the playback position again.
     */
    void resetNotes() { needsResync = true; }

private:
    int64 secondsToSamples(double seconds) const
    {
        return static_cast<int64>(seconds * sampleRate.load());
    }

    int64 getEventPosition(int eventIndex) const
    {
        return secondsToSamples(sequence.getEventPointer(eventIndex)->message.getTimeStamp());
    }

    // Until the device reports its own, so that lengths are meaningful before playback
    std::atomic<double> sampleRate { 44100.0 };

    std::atomic<int64> readPosition { 0 };
    std::atomic<bool> needsResync { true };

    // The first event that has not been played yet, which only the audio thread touches
    int nextEventIndex = 0;

    MidiMessageSequence sequence;
    Synthesiser synth;
};
