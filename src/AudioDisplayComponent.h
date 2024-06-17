#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "MediaDisplayComponent.h"


class AudioDisplayComponent : public MediaDisplayComponent
{
public:
    ~AudioDisplayComponent()
    {
        transportSource.setSource (nullptr);
        sourcePlayer.setSource (nullptr);

        deviceManager.removeAudioCallback (&sourcePlayer);
    }

    void setupDisplay()
    {
        formatManager.registerBasicFormats();

        deviceManager.initialise (0, 2, nullptr, true, {}, nullptr);
        deviceManager.addAudioCallback(&sourcePlayer);

        sourcePlayer.setSource(&transportSource);
    }

    void setZoomFactor(float xScale, float yScale)
    {
        if (thumbnail.getTotalLength() > 0)
            {
                auto newScale = jmax (0.001, thumbnail.getTotalLength() * (1.0 - jlimit (0.0, 0.99, factor)));
                auto timeAtCentre = xToTime ((float) getWidth() / 2.0f);

                setRange ({ timeAtCentre - newScale * 0.5, timeAtCentre + newScale * 0.5 });
            }
    }

    void loadMediaFile(const URL& filePath)
    {
        // unload the previous file source and delete it..
        transportSource.stop();
        transportSource.setSource(nullptr);
        audioFileSource.reset();

        const auto source = std::make_unique<URLInputSource>(filePath);

        File audioFile = filePath.getLocalFile();

        if (source == nullptr)
            DBG("AudioDisplayComponent::loadMediaFile: File " << audioFile.getFullPathName() << " does not exist.");
            // TODO - better error handing
            jassertfalse;

        auto stream = rawToUniquePtr(source->createInputStream());

        if (stream == nullptr)
            DBG("AudioDisplayComponent::loadMediaFile: Failed to load file " << audioFile.getFullPathName() << ".");
            // TODO - better error handing
            jassertfalse;

        auto reader = rawToUniquePtr(formatManager.createReaderFor(std::move(stream)));

        if (reader == nullptr)
            DBG("AudioDisplayComponent::loadMediaFile: Failed to read file " << audioFile.getFullPathName() << ".");
            // TODO - better error handing
            jassertfalse;

        audioFileSource = std::make_unique<AudioFormatReaderSource>(reader.release(), true);

        // ..and plug it into our transport source
        transportSource.setSource (audioFileSource.get(),
                                   32768,                   // tells it to buffer this many samples ahead
                                   &thread,                 // this is the background thread to use for reading-ahead
                                   audioFileSource->getAudioFormatReader()->sampleRate);     // allows for sample rate correction

        zoomSlider.setValue (0, dontSendNotification);
        thumbnail->setURL(getTempFilePath());
        thumbnail->setVisible( true );
        DBG("Set visibility true again");
    }

    void togglePlay()
    {
        if (transportSource.isPlaying())
        {
            // TODO - pause function?
            transportSource.stop();
            transportSource.setPosition (0);
            playStopButton.setMode(playButtonInfo.label);
            stopTimer();
        }
        else
        {
            // transportSource.setPosition (0);
            transportSource.start();
            playStopButton.setMode(stopButtonInfo.label);
            startTimerHz(10);
        }
    }

    bool isPlaying()
    {
        return transportSource.isPlaying();
    }

    double getCurrentPosition()
    {
        return transportSource.getCurrentPosition();
    }

private:
    AudioFormatManager formatManager;
    AudioDeviceManager deviceManager;

    std::unique_ptr<AudioFormatReaderSource> audioFileSource;

    AudioSourcePlayer sourcePlayer;
    AudioTransportSource transportSource;

    AudioThumbnail thumbnail;

    String mediaHandlerInstructions = "Audio waveform.\nClick and drag to start playback from any point in the waveform\nVertical scroll to zoom in/out.\nHorizontal scroll to move the waveform.";
};
