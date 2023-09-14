/**
 * @file
 * @brief Base class for any wave 2 wave models. Wave 2 wave models take samples
 * from an audio source and will output a sample buffer to be read from.
 * @author hugo flores garcia, aldo aguilar
 */
#pragma once

#include <any>
#include <map>
#include <string>

#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_audio_formats/juce_audio_formats.h"

using std::any;
using std::map;
using std::string;

/**
 * @class Wave2Wave
 * @brief Class that represents Wave2Wave Model.
 */
class Wave2Wave {
protected:
  bool save_buffer_to_file(const juce::AudioBuffer<float> &buffer,
                           const juce::File &outputFile, int sampleRate) const {
    juce::WavAudioFormat wavFormat;
    juce::TemporaryFile tempFile(outputFile);
    juce::File tempOutputFile = tempFile.getFile();
    std::unique_ptr<juce::FileOutputStream> outputFileStream(
        tempOutputFile.createOutputStream());

    if (outputFileStream == nullptr) {
      DBG("Failed to create output stream.");
      return false;
    }

    // TODO: should be unique ptr
    auto writer =
        std::unique_ptr<juce::AudioFormatWriter>(wavFormat.createWriterFor(
            outputFileStream.get(), sampleRate,
            static_cast<unsigned int>(buffer.getNumChannels()), 16, {}, 0));

    if (writer == nullptr) {
      DBG("Failed to create audio format writer.");
      return false;
    }

    outputFileStream
        .release(); // The writer will take ownership of the output stream
    bool success =
        writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());

    if (!success) {
      DBG("Failed to write audio buffer to file.");
      return false;
    }

    writer.reset(); // Release the writer before replacing the file
    if (tempFile.overwriteTargetFileWithTemporary()) {
      DBG("Audio buffer saved to file.");
      return true;
    } else {
      return false;
    }
  }

  bool load_buffer_from_file(const juce::File &inputFile,
                             juce::AudioBuffer<float> &buffer,
                             int targetSampleRate) const {
    juce::WavAudioFormat wavFormat;

    // NOTE: the input stream will be destroyed by the AudioFormatReader.
    auto *reader =
        wavFormat.createReaderFor(new juce::FileInputStream(inputFile), true);
    if (reader == nullptr) {
      DBG("Failed to create audio format reader.");
      return false;
    }
    DBG("Loaded audio file with sample rate: " +
        std::to_string(reader->sampleRate));

    auto *readerSource = new juce::AudioFormatReaderSource(reader, true);
    auto resamplingSource =
        std::make_unique<juce::ResamplingAudioSource>(readerSource, true, 2);

    double resamplingRatio = static_cast<double>(targetSampleRate) /
                            static_cast<double>(reader->sampleRate);
    resamplingSource->setResamplingRatio(resamplingRatio);
    resamplingSource->prepareToPlay(reader->lengthInSamples, reader->sampleRate);
    DBG("Resampling ratio: " + std::to_string(resamplingRatio));

    // resize the buffer to account for resampling
    int resampledBufferLength = static_cast<int>(
        static_cast<double>(reader->lengthInSamples) * resamplingRatio);
    DBG("Resampled buffer length: " + std::to_string(resampledBufferLength));

    DBG("resizing buffer to " + std::to_string(reader->numChannels) +
        " channels and " + std::to_string(resampledBufferLength) + " samples");
    buffer.setSize(reader->numChannels, resampledBufferLength);

    resamplingSource->getNextAudioBlock(juce::AudioSourceChannelInfo(buffer));

    return true;
  }

public:
  /**
   * @brief Processes a buffer of audio data with the model.
   * @param bufferToProcess Buffer to be processed by the model.
   * @param sampleRate The sample rate of the audio data.
   * @param kwargs A map of parameters for the model.
   */
  virtual void process(juce::AudioBuffer<float> *bufferToProcess,
                       int sampleRate,
                       const map<string, any> &kwargs) const = 0;
};
