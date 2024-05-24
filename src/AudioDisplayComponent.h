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

        thumbnail.setFollowsTransport(followCursorButton.getToggleState());
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
        // TODO - expecting URL, not URL&

        // unload the previous file source and delete it..
        transportSource.stop();
        transportSource.setSource (nullptr);
        currentAudioFileSource.reset();

        const auto source = makeInputSource (audioURL);

        if (source == nullptr)
            return false;

        auto stream = rawToUniquePtr (source->createInputStream());

        if (stream == nullptr)
            return false;

        auto reader = rawToUniquePtr (formatManager.createReaderFor (std::move (stream)));

        if (reader == nullptr)
            return false;

        currentAudioFileSource = std::make_unique<AudioFormatReaderSource> (reader.release(), true);

        // ..and plug it into our transport source
        transportSource.setSource (currentAudioFileSource.get(),
                                   32768,                   // tells it to buffer this many samples ahead
                                   &thread,                 // this is the background thread to use for reading-ahead
                                   currentAudioFileSource->getAudioFormatReader()->sampleRate);     // allows for sample rate correction

        return true;

        if (! loadURLIntoTransport(filePath))
        {
            // Failed to load the audio file!
            jassertfalse;
            return;
        }

        zoomSlider.setValue (0, dontSendNotification);
        thumbnail->setURL (currentMediaFile);
        thumbnail->setVisible( true );
        DBG("Set visibility true again");
    }

    void togglePlay()
    {
        if (transportSource.isPlaying())
        {
            transportSource.stop();
        }
        else
        {
            transportSource.setPosition(0);
            transportSource.start();
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
    AudioSourcePlayer sourcePlayer;
    AudioTransportSource transportSource;
    AudioThumbnail thumbnail;

    std::unique_ptr<AudioFormatReaderSource> currentAudioFileSource;

    AudioDeviceManager deviceManager;

    AudioFormatManager formatManager;
};
