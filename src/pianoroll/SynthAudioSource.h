#include <juce_audio_basics/juce_audio_basics.h>

struct SineWaveSound : public juce::SynthesiserSound
{
    SineWaveSound() {}

    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

struct SineWaveVoice : public juce::SynthesiserVoice
{
    SineWaveVoice() {}

    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<SineWaveSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber,
                   float velocity,
                   juce::SynthesiserSound*,
                   int /*currentPitchWheelPosition*/) override
    {
        currentAngle = 0.0;
        level = velocity * 0.15;
        tailOff = 0.0;

        auto cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        auto cyclesPerSample = cyclesPerSecond / getSampleRate();

        angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;
    }

    void stopNote(float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            if (tailOff == 0.0)
                tailOff = 1.0;
        }
        else
        {
            clearCurrentNote();
            angleDelta = 0.0;
        }
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioSampleBuffer& outputBuffer,
                         int startSample,
                         int numSamples) override
    {
        if (angleDelta != 0.0)
        {
            if (tailOff > 0.0) // [7]
            {
                while (--numSamples >= 0)
                {
                    auto currentSample = (float) (std::sin(currentAngle) * level * tailOff);

                    for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                        outputBuffer.addSample(i, startSample, currentSample);

                    currentAngle += angleDelta;
                    ++startSample;

                    tailOff *= 0.99; // [8]

                    if (tailOff <= 0.005)
                    {
                        clearCurrentNote(); // [9]

                        angleDelta = 0.0;
                        break;
                    }
                }
            }
            else
            {
                while (--numSamples >= 0) // [6]
                {
                    auto currentSample = (float) (std::sin(currentAngle) * level);

                    for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                        outputBuffer.addSample(i, startSample, currentSample);

                    currentAngle += angleDelta;
                    ++startSample;
                }
            }
        }
    }

private:
    double currentAngle = 0.0, angleDelta = 0.0, level = 0.0, tailOff = 0.0;
};

class SynthAudioSource : public juce::PositionableAudioSource
{
public:
    SynthAudioSource()
    {
        synth.addSound(new SineWaveSound()); // [2]
    }

    void setUsingSineWaveSound() { synth.clearSounds(); }

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        DBG("Sample rate being set to " << sampleRate);
        DBG("Samples per block being set to " << samplesPerBlockExpected);
        synth.setCurrentPlaybackSampleRate(sampleRate); // [3]
        mySampleRate = sampleRate;
        samplesPerBlock = samplesPerBlockExpected;
    }

    void releaseResources() override {}

    void useSequence(juce::MidiMessageSequence midiSequence)
    {
        sequence = midiSequence;

        // Get max number of voices needed and add that many voices
        int maxVoices = 0;
        int currVoices = 0;
        for (int eventIdx = 0; eventIdx < sequence.getNumEvents(); ++eventIdx)
        {
            const auto midiEvent = sequence.getEventPointer(eventIdx);
            const auto& midiMessage = midiEvent->message;

            double startTime = midiEvent->message.getTimeStamp();
            lastStartTime = startTime;

            // DBG("Event " << eventIdx << " at " << startTime << ": " << midiMessage.getDescription());

            if (midiMessage.isNoteOn())
            {
                currVoices++;
                if (currVoices > maxVoices)
                {
                    maxVoices = currVoices;
                    DBG("Max voices now " << maxVoices);
                }
            }

            if (midiMessage.isNoteOff())
            {
                currVoices--;
            }
            // DBG("Curr voices is " << currVoices);
        }

        synth.clearVoices();

        for (auto i = 0; i < maxVoices; ++i) // [1]
            synth.addVoice(new SineWaveVoice());
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        bufferToFill.clearActiveBufferRegion();

        juce::MidiBuffer incomingMidi;

        int lastSample = readPosition + samplesPerBlock;

        for (int eventIdx = 0; eventIdx < sequence.getNumEvents(); ++eventIdx)
        {
            const auto midiEvent = sequence.getEventPointer(eventIdx);
            const auto& midiMessage = midiEvent->message;

            int startTimeSamples = secondsToSamples(midiEvent->message.getTimeStamp());

            if (startTimeSamples >= readPosition && startTimeSamples <= lastSample)
            {
                incomingMidi.addEvent(midiMessage, startTimeSamples - readPosition);

                // DBG("Event " << eventIdx << " at " << startTimeSamples << ": " << midiMessage.getDescription());
            }
        }

        synth.renderNextBlock(*bufferToFill.buffer,
                              incomingMidi,
                              bufferToFill.startSample,
                              bufferToFill.numSamples); // [5]

        readPosition += samplesPerBlock;
    }

    void setNextReadPosition(int64 newPosition) override { readPosition = newPosition; }

    int64 getNextReadPosition() const override { return readPosition; }

    int64 getTotalLength() const override
    {
        // Location of last MIDI message (will likely be a note off)
        return secondsToSamples(lastStartTime);
    }

    bool isLooping() const override
    {
        // TODO
        return 0;
    }

    void setLooping(bool shouldLoop) override
    {
        // TODO
    }

    void resetNotes() { synth.allNotesOff(0, false); }

private:
    juce::Synthesiser synth;
    juce::MidiMessageSequence sequence;

    double mySampleRate;
    int samplesPerBlock;

    double lastStartTime;

    int64 readPosition;

    int secondsToSamples(double secondsTime) const { return (int) (secondsTime * mySampleRate); }
};
