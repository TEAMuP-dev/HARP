#include "AudioDisplayComponent.h"

#include "../utils/AudioFiles.h"

AudioDisplayComponent::AudioDisplayComponent() : AudioDisplayComponent("Audio Track") {}

AudioDisplayComponent::AudioDisplayComponent(String name, bool req, bool fromDAW, DisplayMode mode)
    : MediaDisplayComponent(name, req, fromDAW, mode)
{
    thread.startThread(Thread::Priority::normal);

    // Need to repaint when visible range changes
    thumbnail.addChangeListener(this);

    thumbnailComponent.addMouseListener(this, true);
    contentComponent.addAndMakeVisible(thumbnailComponent);

    mediaInstructions = "Audio waveform.\nClick or drag to move the playback position.\n"
                        "Scroll to zoom in time, or hold Shift while scrolling to pan.";
}

AudioDisplayComponent::~AudioDisplayComponent()
{
    resetTransport();

    thumbnailComponent.removeMouseListener(this);
    thumbnail.removeChangeListener(this);
}

StringArray AudioDisplayComponent::getSupportedExtensions()
{
    // The formats every platform reads, then the ones the decoders of this platform add
    StringArray extensions = AudioFiles::portableExtensions();
    extensions.addArray(AudioFiles::platformExtensions());

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
    File audioFile = filePath.getLocalFile();

    auto stream = rawToUniquePtr(URLInputSource(filePath).createInputStream());

    // Left empty if the file cannot be read, as a file of silence would be
    if (stream == nullptr)
    {
        DBG_AND_LOG("AudioDisplayComponent::loadMediaFile: Failed to load file "
                    << audioFile.getFullPathName() << ".");
        return;
    }

    auto reader = rawToUniquePtr(formatManager.createReaderFor(std::move(stream)));

    if (reader == nullptr)
    {
        DBG_AND_LOG("AudioDisplayComponent::loadMediaFile: Failed to read file "
                    << audioFile.getFullPathName() << ".");
        return;
    }

    audioFileSource = std::make_unique<AudioFormatReaderSource>(reader.release(), true);

    transportSource.setSource(
        audioFileSource.get(),
        32768, // Amount of samples to buffer ahead
        &thread, // Thread to use for reading-ahead
        audioFileSource->getAudioFormatReader()->sampleRate); // Allows for sample rate correction
}

void AudioDisplayComponent::resetMedia()
{
    resetTransport();

    audioFileSource.reset();
    thumbnail.clear();
}

void AudioDisplayComponent::postLoadActions(const URL& filePath)
{
    thumbnailCache.clear();
    thumbnail.setSource(new URLInputSource(filePath));
}
