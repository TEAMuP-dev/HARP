#include "AudioDisplayComponent.h"

AudioDisplayComponent::AudioDisplayComponent() : AudioDisplayComponent("Audio Track") {}

AudioDisplayComponent::AudioDisplayComponent(String name, bool req, bool fromDAW, DisplayMode mode)
    : MediaDisplayComponent(name, req, fromDAW, mode)
{
    thread.startThread(Thread::Priority::normal);

    // Need to repaint when visible range changes
    thumbnail.addChangeListener(this);

    thumbnailComponent.addMouseListener(this, true);
    contentComponent.addAndMakeVisible(thumbnailComponent);

    mediaInstructions =
        "Audio waveform.\nClick and drag to start playback from any point in the waveform\nVertical scroll to zoom in/out.\nHorizontal scroll to move the waveform.";
}

AudioDisplayComponent::~AudioDisplayComponent()
{
    resetTransport();

    thumbnailComponent.removeMouseListener(this);
    thumbnail.removeChangeListener(this);
}

StringArray AudioDisplayComponent::getSupportedExtensions()
{
    StringArray extensions;

    extensions.add(".wav");
    extensions.add(".bwf");
    extensions.add(".aiff");
    extensions.add(".aif");
    extensions.add(".flac");
    extensions.add(".ogg");
    extensions.add(".mp3");

    return extensions;
}

void AudioDisplayComponent::resized()
{
    MediaDisplayComponent::resized();

    // Set thumbnail to fill entire media content area
    thumbnailComponent.setBounds(contentComponent.getLocalBounds());
}

void AudioDisplayComponent::loadMediaFile(const URL& filePath)
{
    const auto source = std::make_unique<URLInputSource>(filePath);

    File audioFile = filePath.getLocalFile();

    if (source == nullptr)
    {
        DBG("AudioDisplayComponent::loadMediaFile: File " << audioFile.getFullPathName()
                                                          << " does not exist.");
        // TODO - better error handing
        jassertfalse;
        return;
    }

    auto stream = rawToUniquePtr(source->createInputStream());

    if (stream == nullptr)
    {
        DBG("AudioDisplayComponent::loadMediaFile: Failed to load file "
            << audioFile.getFullPathName() << ".");
        // TODO - better error handing
        jassertfalse;
        return;
    }

    auto reader = rawToUniquePtr(formatManager.createReaderFor(std::move(stream)));

    if (reader == nullptr)
    {
        DBG("AudioDisplayComponent::loadMediaFile: Failed to read file "
            << audioFile.getFullPathName() << ".");
        // TODO - better error handing
        jassertfalse;
        return;
    }

    audioFileSource = std::make_unique<AudioFormatReaderSource>(reader.release(), true);

    transportSource.setSource(
        audioFileSource.get(),
        32768, // Amount of samples to buffer ahead
        &thread, // Thread to use for reading-ahead
        audioFileSource->getAudioFormatReader()->sampleRate); // Allows for sample rate correction

    // Calculate horizontal center of mass (weighted average of amplitude)
    AudioFormatReader* readerPtr = audioFileSource->getAudioFormatReader();
    if (readerPtr != nullptr)
    {
        double sumTimeAmp = 0.0;
        double sumAmp = 0.0;

        int numChannels = readerPtr->numChannels;
        double sampleRate = readerPtr->sampleRate;
        int64 lengthInSamples = readerPtr->lengthInSamples;

        // Use a large block size for scanning to be fast
        int blockSize = 1024;
        AudioBuffer<float> buffer(numChannels, blockSize);

        int64 position = 0;
        while (position < lengthInSamples)
        {
            int numSamplesToRead =
                static_cast<int>(jmin(static_cast<int64>(blockSize), lengthInSamples - position));
            readerPtr->read(&buffer, 0, numSamplesToRead, position, true, true);

            for (int i = 0; i < numSamplesToRead; ++i)
            {
                float amp = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    amp += std::abs(buffer.getSample(ch, i));
                }
                amp /= static_cast<float>(numChannels); // Average across channels

                double time = static_cast<double>(position + i) / sampleRate;

                sumTimeAmp += time * amp;
                sumAmp += amp;
            }

            position += numSamplesToRead;
        }

        if (sumAmp > 0.0001)
        {
            horizontalCenterOfMass = sumTimeAmp / sumAmp;
        }
        else
        {
            horizontalCenterOfMass = 0.0;
        }
    }
}

void AudioDisplayComponent::resetMedia()
{
    resetTransport();

    audioFileSource.reset();
    thumbnail.clear();
}

void AudioDisplayComponent::postLoadActions(const URL& filePath)
{
    if (auto inputSource = std::make_unique<URLInputSource>(filePath))
    {
        thumbnailCache.clear();
        thumbnail.setSource(inputSource.release());

        // Auto-center horizontally about weighted average amplitude
        autoZoomToHorizontalMass();
    }
}
