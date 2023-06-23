/**
  * @file
  * @brief This file is part of the JUCE examples.
  * 
  * Copyright (c) 2022 - Raw Material Software Limited
  * The code included in this file is provided under the terms of the ISC license
  * http://www.isc.org/downloads/software-support-policy/isc-license. Permission
  * To use, copy, modify, and/or distribute this software for any purpose with or
  * without fee is hereby granted provided that the above copyright notice and
  * this permission notice appear in all copies.
  * 
  * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
  * WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
  * PURPOSE, ARE DISCLAIMED.
  * 
  * @brief Models defined in this file are any audio processing models that utilize a libtorch backend
  * for processing data. 
  * @author hugo flores garcia, aldo aguilar
  */
#include "Wave2Wave.h"

bool Wave2Wave::save_buffer_to_file(
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

bool Wave2Wave::load_buffer_from_file(
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
