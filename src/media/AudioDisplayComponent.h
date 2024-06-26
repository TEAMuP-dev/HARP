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
    }

    void setupDisplay()
    {
        thread.startThread (Thread::Priority::normal);

        formatManager.registerBasicFormats();

        deviceManager.initialise (0, 2, nullptr, true, {}, nullptr);
        deviceManager.addAudioCallback(&sourcePlayer);

        sourcePlayer.setSource(&transportSource);

        mediaHandlerInstructions = "Audio waveform.\nClick and drag to start playback from any point in the waveform\nVertical scroll to zoom in/out.\nHorizontal scroll to move the waveform.";
    }

    static StringArray getSupportedExtensions()
    {
        // TODO
    }

    void loadMediaFile(const URL& filePath)
    {
        setNewTarget(filePath);

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
                                   32768,                   // tells it to buffer this many samples ahead
                                   &thread,                 // this is the background thread to use for reading-ahead
                                   audioFileSource->getAudioFormatReader()->sampleRate);     // allows for sample rate correction

        //zoomSlider.setValue(0, dontSendNotification);
        //thumbnail->setURL(getTempFilePath());
        //thumbnail->setVisible(true);
        DBG("Set visibility true again");
    }

    void startPlaying()
    {
        transportSource.start();
    }

    void stopPlaying()
    {
        transportSource.stop();
        transportSource.setPosition(0);
    }

private:

    AudioFormatManager formatManager;
    AudioDeviceManager deviceManager;

    std::unique_ptr<AudioFormatReaderSource> audioFileSource;

    AudioSourcePlayer sourcePlayer;
    AudioTransportSource transportSource;

    AudioThumbnailCache thumbnailCache{ 5 };

    AudioThumbnail thumbnail = AudioThumbnail(512, formatManager, thumbnailCache);;

    TimeSliceThread thread{ "audio file preview" };
};
