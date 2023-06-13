#pragma once

#include <string>
#include <any>
#include <unordered_map>

#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_audio_formats/juce_audio_formats.h"



using std::map;
using std::string;
using std::any;

// python-like dict using a std::map underneath. 
// returns std::nullopt if the key is not found.
// templates allow the keys and values to be of any type




// abstract class for different types of deep learning processors. 
class Model {
public:
    virtual bool load(const map<string, any> &params) = 0;
    virtual bool ready() const = 0;

};


class Wave2Wave {
protected:
    bool save_buffer_to_file(
        const juce::AudioBuffer<float> &buffer, 
        const juce::File& outputFile, 
        int sampleRate
    ) const {
        juce::WavAudioFormat wavFormat;
        juce::TemporaryFile tempFile(outputFile);
        juce::File tempOutputFile = tempFile.getFile();
        std::unique_ptr<juce::FileOutputStream> outputFileStream(tempOutputFile.createOutputStream());

        if (outputFileStream == nullptr)
        {
            DBG("Failed to create output stream.");
            return false;
        }


        // TODO: should be unique ptr
        auto writer = std::unique_ptr<juce::AudioFormatWriter>(wavFormat.createWriterFor(
            outputFileStream.get(),
            sampleRate,
            static_cast<unsigned int>(buffer.getNumChannels()),
            16,
            {},
            0
        ));


        if (writer == nullptr)
        {
            DBG("Failed to create audio format writer.");
            return false;
        }

        outputFileStream.release(); // The writer will take ownership of the output stream
        bool success = writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());

        if (!success)
        {
            DBG("Failed to write audio buffer to file.");
            return false;
        }

        writer.reset(); // Release the writer before replacing the file
        if (tempFile.overwriteTargetFileWithTemporary())
        {
            DBG("Audio buffer saved to file.");
            return true;
        }
        else
        {
            return false;
        }

    }

    bool load_buffer_from_file(
        const juce::File& inputFile,
        juce::AudioBuffer<float> &buffer,
        int targetSampleRate
    ) const {
        juce::WavAudioFormat wavFormat;

        // NOTE: the input stream will be destroyed by the AudioFormatReader. 
        auto *reader = wavFormat.createReaderFor(
                new juce::FileInputStream(inputFile), true
        );
        if (reader == nullptr)
        {
            DBG("Failed to create audio format reader.");
            return false;
        }
        DBG("Loaded audio file with sample rate: " + std::to_string(reader->sampleRate));

        auto *readerSource = new juce::AudioFormatReaderSource(reader, true);
        auto resamplingSource = std::make_unique<juce::ResamplingAudioSource>(
                readerSource, true, 2
        );

        double resamplingRatio = static_cast<double>(targetSampleRate) / static_cast<double>(reader->sampleRate);
        resamplingSource->setResamplingRatio(resamplingRatio);
        resamplingSource->prepareToPlay(reader->lengthInSamples, reader->sampleRate);
        DBG("Resampling ratio: " + std::to_string(resamplingRatio));

        // resize the buffer to account for resampling
        int resampledBufferLength = static_cast<int>(
            static_cast<double>(reader->lengthInSamples) 
            * resamplingRatio
        );
        DBG("Resampled buffer length: " + std::to_string(resampledBufferLength));

        DBG("resizing buffer to " + std::to_string(reader->numChannels) + " channels and " + std::to_string(resampledBufferLength) + " samples");
        buffer.setSize(reader->numChannels, resampledBufferLength);

        resamplingSource->getNextAudioBlock(juce::AudioSourceChannelInfo(buffer));

        return true;
    }

public:

    // process a buffer of audio data with the model. 
    // data should be written to the bufferToProcess
    // TODO: buffer should be pass by ref
    virtual void process(
        juce::AudioBuffer<float> *bufferToProcess, 
        int sampleRate,
        const map<string, any> &kwargs
    ) const = 0;
};