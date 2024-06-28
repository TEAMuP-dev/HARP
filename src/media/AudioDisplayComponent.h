#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "MediaDisplayComponent.h"


class AudioDisplayComponent : public MediaDisplayComponent
{
public:

    ~AudioDisplayComponent()
    {
        deviceManager.removeAudioCallback(&sourcePlayer);

        transportSource.setSource(nullptr);
        sourcePlayer.setSource(nullptr);

        thumbnail.removeChangeListener(this);
        //scrollbar.removeListener(this);
    }

    void setupDisplay()
    {
        thread.startThread (Thread::Priority::normal);

        formatManager.registerBasicFormats();

        deviceManager.initialise (0, 2, nullptr, true, {}, nullptr);
        deviceManager.addAudioCallback(&sourcePlayer);

        sourcePlayer.setSource(&transportSource);

        thumbnail.addChangeListener (this);

        mediaHandlerInstructions = "Audio waveform.\nClick and drag to start playback from any point in the waveform\nVertical scroll to zoom in/out.\nHorizontal scroll to move the waveform.";
    }

    void drawMainArea(Graphics& g, Rectangle<int> a)
    {
        thumbnail.drawChannels(g, a.reduced(2), visibleRange.getStart(), visibleRange.getEnd(), 1.0f);
    }

    static StringArray getSupportedExtensions()
    {
        // TODO
    }

    void loadMediaFile(const URL& filePath)
    {
        // unload the previous file source and delete it..
        transportSource.stop();
        transportSource.setSource(nullptr);
        audioFileSource.reset();

        const auto source = std::make_unique<URLInputSource>(filePath);

        File audioFile = filePath.getLocalFile();

        if (source == nullptr) {
            DBG("AudioDisplayComponent::loadMediaFile: File " << audioFile.getFullPathName() << " does not exist.");
            // TODO - better error handing
            jassertfalse;
            return;
        }

        auto stream = rawToUniquePtr(source->createInputStream());

        if (stream == nullptr) {
            DBG("AudioDisplayComponent::loadMediaFile: Failed to load file " << audioFile.getFullPathName() << ".");
            // TODO - better error handing
            jassertfalse;
            return;
        }

        auto reader = rawToUniquePtr(formatManager.createReaderFor(std::move(stream)));

        if (reader == nullptr) {
            DBG("AudioDisplayComponent::loadMediaFile: Failed to read file " << audioFile.getFullPathName() << ".");
            // TODO - better error handing
            jassertfalse;
            return;
        }

        audioFileSource = std::make_unique<AudioFormatReaderSource>(reader.release(), true);

        // ..and plug it into our transport source
        transportSource.setSource (audioFileSource.get(),
                                   32768, // tells it to buffer this many samples ahead
                                   &thread, // this is the background thread to use for reading-ahead
                                   audioFileSource->getAudioFormatReader()->sampleRate); // allows for sample rate correction
    }

    void setPlaybackPosition(float x) { transportSource.setPosition(jmax(0.0, xToTime(x))); }

    float getPlaybackPosition() { transportSource.getCurrentPosition(); }


    void startPlaying()
    {
        transportSource.start();
    }

    void stopPlaying()
    {
        transportSource.stop();
        transportSource.setPosition(0); // TODO?
    }

    bool isPlaying() { return transportSource.isPlaying(); }

    double getTotalLengthInSecs()
    {
        return thumbnail.getTotalLength();
    }

private:

    void postLoadMediaActions(const URL& filePath)
    {
        if (auto inputSource = std::make_unique<URLInputSource>(filePath)) {
            thumbnailCache.clear();
            thumbnail.setSource(inputSource.release());
        }
    }

    AudioFormatManager formatManager;
    AudioDeviceManager deviceManager;

    std::unique_ptr<AudioFormatReaderSource> audioFileSource;

    AudioSourcePlayer sourcePlayer;
    AudioTransportSource transportSource;

    AudioThumbnailCache thumbnailCache{ 5 };

    AudioThumbnail thumbnail = AudioThumbnail(512, formatManager, thumbnailCache);

    TimeSliceThread thread{ "audio file preview" };
};
