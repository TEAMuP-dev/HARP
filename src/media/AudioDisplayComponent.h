#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "MediaDisplayComponent.h"


class AudioDisplayComponent : public MediaDisplayComponent
{
public:

    ~AudioDisplayComponent()
    {
        // TODO
    }

    void setupDisplay()
    {
        // TODO
    }

    static StringArray getSupportedExtensions()
    {
        StringArray extensions;
        for (int i = 0; i < formatManager.getNumKnownFormats(); ++i)
        {
            auto* format = formatManager.getKnownFormat(i);
            auto formatExtensions = format->getFileExtensions();
            for (auto& ext : formatExtensions)
            {
                extensions.addTokens("*" + ext.trim(), ";", "\"");
            }
            // extensions.addTokens(format->getFileExtensions(), ";", "\"" );
        }
        extensions.removeDuplicates(false);
        return extensions.joinIntoString(";");
    }

    std::string getAllAudioFileExtensions2(AudioFormatManager& formatManager)
    {
        std::set<std::string> uniqueExtensions;

        for (int i = 0; i < formatManager.getNumKnownFormats(); ++i)
        {
            auto* format = formatManager.getKnownFormat(i);
            juce::String extensionsString = format->getFileExtensions()[0];
            StringArray extensions = StringArray::fromTokens(extensionsString, ";", "");
            // StringArray extensions = StringArray::fromTokens(format->getFileExtensions(), ";", "");

            for (auto& ext : extensions)
            {
                uniqueExtensions.insert(ext.toStdString());
            }
        }

        // Join all extensions into a single string with semicolons
        std::string allExtensions;
        for (auto it = uniqueExtensions.begin(); it != uniqueExtensions.end(); ++it)
        {
            if (it != uniqueExtensions.begin()) {
                allExtensions += ";";
            }
            allExtensions += *it;
        }

        return allExtensions;
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

        if (source == nullptr)
            DBG("AudioDisplayComponent::loadMediaFile: File " << audioFile.getFullPathName() << " does not exist.");
            // TODO - better error handing
            jassertfalse;
            return

        auto stream = rawToUniquePtr(source->createInputStream());

        if (stream == nullptr)
            DBG("AudioDisplayComponent::loadMediaFile: Failed to load file " << audioFile.getFullPathName() << ".");
            // TODO - better error handing
            jassertfalse;
            return

        auto reader = rawToUniquePtr(formatManager.createReaderFor(std::move(stream)));

        if (reader == nullptr)
            DBG("AudioDisplayComponent::loadMediaFile: Failed to read file " << audioFile.getFullPathName() << ".");
            // TODO - better error handing
            jassertfalse;
            return

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
        // TODO
    }

    void stopPlaying()
    {
        // TODO
    }

private:

};
