// See https://juce.com/tutorials/tutorial_synth_using_midi_input/ for details

#include <juce_audio_basics/juce_audio_basics.h>

struct SineWaveSound : public SynthesiserSound
{
    SineWaveSound() {}

    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

struct SineWaveVoice : public SynthesiserVoice
{
    SineWaveVoice() {}

    bool canPlaySound(SynthesiserSound* sound) override
    {
        return dynamic_cast<SineWaveSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber,
                   float velocity,
                   SynthesiserSound*,
                   int /*currentPitchWheelPosition*/) override
    {
        currentAngle = 0.0;

        double cyclesPerSecond = MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        double cyclesPerSample = cyclesPerSecond / getSampleRate();

        angleDelta = cyclesPerSample * 2.0 * MathConstants<double>::pi;

        tailOff = 0.0;
        level = static_cast<double>(velocity) * 0.15;
    }

    void stopNote(float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            if (tailOff == 0.0)
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
        if (angleDelta != 0.0)
        {
            if (tailOff > 0.0)
            {
                while (--numSamples >= 0)
                {
                    float currentSample =
                        static_cast<float>(std::sin(currentAngle) * level * tailOff);

                    for (int i = outputBuffer.getNumChannels(); --i >= 0;)
                    {
                        outputBuffer.addSample(i, startSample, currentSample);
                    }

                    currentAngle += angleDelta;

                    ++startSample;

                    tailOff *= 0.99;

                    if (tailOff <= 0.005)
                    {
                        clearCurrentNote();

                        angleDelta = 0.0;

                        break;
                    }
                }
            }
            else
            {
                while (--numSamples >= 0)
                {
                    auto currentSample = static_cast<float>(std::sin(currentAngle) * level);

                    for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                    {
                        outputBuffer.addSample(i, startSample, currentSample);
                    }

                    currentAngle += angleDelta;

                    ++startSample;
                }
            }
        }
    }

private:
    double currentAngle = 0.0;
    double angleDelta = 0.0;
    double tailOff = 0.0;
    double level = 0.0;
};

class SynthAudioSource : public PositionableAudioSource
{
public:
    SynthAudioSource() { synth.addSound(new SineWaveSound()); }

    void setUsingSineWaveSound() { synth.clearSounds(); }

    void prepareToPlay(int samplesPerBlockExpected, double sr) override
    {
        //DBG("SynthAudioSource::prepareToPlay: Sample rate being set to " << sr << ".");
        //DBG("SynthAudioSource::prepareToPlay: Samples per block being set to "
        //    << samplesPerBlockExpected << ".");

        synth.setCurrentPlaybackSampleRate(sr);
        sampleRate = sr;
        samplesPerBlock = samplesPerBlockExpected;
    }

    void releaseResources() override {}

    void useSequence(MidiMessageSequence midiSequence)
    {
        sequence = midiSequence;

        int maxVoices = 0;
        int currVoices = 0;

        // Get max number of voices needed and add that many voices
        for (int eventIdx = 0; eventIdx < sequence.getNumEvents(); ++eventIdx)
        {
            const auto midiEvent = sequence.getEventPointer(eventIdx);
            const auto& midiMessage = midiEvent->message;

            double startTime = midiEvent->message.getTimeStamp();

            lastStartTime = startTime;

            if (midiMessage.isNoteOn())
            {
                currVoices++;
                if (currVoices > maxVoices)
                {
                    maxVoices = currVoices;
                }
            }

            if (midiMessage.isNoteOff())
            {
                currVoices--;
            }
        }

        //DBG("SynthAudioSource::useSequence Max voices set to " << maxVoices << ".");

        synth.clearVoices();

        for (auto i = 0; i < maxVoices; ++i)
        {
            synth.addVoice(new SineWaveVoice());
        }
    }

    void getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill) override
    {
        bufferToFill.clearActiveBufferRegion();

        MidiBuffer incomingMidi;

        int64 lastSample = readPosition + samplesPerBlock;

        for (int eventIdx = 0; eventIdx < sequence.getNumEvents(); ++eventIdx)
        {
            const auto midiEvent = sequence.getEventPointer(eventIdx);
            const auto& midiMessage = midiEvent->message;

            int startTimeSamples = secondsToSamples(midiEvent->message.getTimeStamp());

            if (startTimeSamples >= readPosition && startTimeSamples <= lastSample)
            {
                incomingMidi.addEvent(midiMessage,
                                      static_cast<int>(startTimeSamples - readPosition));
            }
        }

        synth.renderNextBlock(
            *bufferToFill.buffer, incomingMidi, bufferToFill.startSample, bufferToFill.numSamples);

        readPosition += samplesPerBlock;
    }

    void setNextReadPosition(int64 newPosition) override { readPosition = newPosition; }

    int64 getNextReadPosition() const override { return readPosition; }

    int64 getTotalLength() const override
    {
        // Location of last MIDI message (likely a note off event)
        return secondsToSamples(lastStartTime);
    }

    void setLooping(bool /*shouldLoop*/) override {}
    bool isLooping() const override { return 0; }

    void resetNotes() { synth.allNotesOff(0, false); }

private:
    int secondsToSamples(double secondsTime) const { return (int) (secondsTime * sampleRate); }

    double sampleRate;
    int samplesPerBlock;
    int64 readPosition;

    double lastStartTime;

    MidiMessageSequence sequence;
    Synthesiser synth;
};
