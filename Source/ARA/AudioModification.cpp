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
 * Audio modification class started from the ARAPluginDemo.h file provided by
 * juce. This class now also manages offline processing.
 * @author JUCE, aldo aguilar, hugo flores garcia, xribene
 */

#include "AudioModification.h"

/**
 * @brief Construct a new AudioModification object
 *
 * @param audioSource Pointer to the ARAAudioSource
 * @param hostRef A reference to ARAAudioModificationHost
 * @param optionalModificationToClone A pointer to an optional modification to
 * clone
 */
AudioModification::AudioModification(
    juce::ARAAudioSource *audioSource, ARA::ARAAudioModificationHostRef hostRef,
    const ARAAudioModification *optionalModificationToClone)
    : ARAAudioModification(audioSource, hostRef, optionalModificationToClone){

  DBG("AudioModification::created");
  DBG("AudioModification::the audio source is " << audioSource->getName());
  DBG("AudioModification:: create reader for " << audioSource->getName());
  mAudioSourceReader = std::make_unique<juce::ARAAudioSourceReader>(audioSource);

//   readMaxLevels (int64 startSample, int64 numSamples,
//                                 Range<float>* results, int numChannelsToRead);
    juce::Range<float> results;
    mAudioSourceReader->readMaxLevels(0, mAudioSourceReader->lengthInSamples, &results, 1);
    DBG("AudioModification:: max level: " << results.getStart() << " " << results.getEnd());
  mSampleRate = audioSource->getSampleRate();
  mAudioSourceName = audioSource->getName();
  // Initialization, it 'll change when the process method is called
  mDawSampleRate = 44100;
}


bool AudioModification::isThumbCreated() const { return mThumbCreated; }

void AudioModification::setThumbCreated(bool created) {
  mThumbCreated = created;
}

int AudioModification::getDawSampleRate() const {
  return mDawSampleRate;
}

std::string AudioModification::getSourceName() { return mAudioSourceName; }

/**
 * @brief Process audio from audio source with deep learning effect
 *
 * @param params Map of parameters for learner
 */
void AudioModification::process(std::shared_ptr<WebWave2Wave> model, double dawSampleRate) {
  mDawSampleRate = dawSampleRate;
  if (model == nullptr) {
    throw std::runtime_error("AudioModification::process: model is null");
    return;
  }

  if (!model->ready()) {
    throw std::runtime_error("AudioModification::process: model is not ready");
    return;
  }

  if (!mAudioSourceReader->isValid())
    DBG("AudioModification:: invalid audio source reader");
  else {
    auto numChannels = mAudioSourceReader->numChannels;
    auto numSamples = mAudioSourceReader->lengthInSamples;
    auto sourceSampleRate = mSampleRate;

    DBG("AudioModification:: audio source: "
        << mAudioSourceName << " channels: " << juce::String(numChannels)
        << " length in samples: " << juce::String(numSamples));

    mAudioBuffer.reset(new juce::AudioBuffer<float>(numChannels, numSamples));

    // reading into audio buffer

    juce::Range<float> results;
    mAudioSourceReader->readMaxLevels(0, mAudioSourceReader->lengthInSamples, &results, 1);
    DBG("AudioModification:: max level: " << results.getStart() << " " << results.getEnd());
    DBG("is valid audio source reader: ", mAudioSourceReader->isValid());
    mAudioSourceReader->read(mAudioBuffer.get(), 0,
                             static_cast<int>(numSamples), 0, true, true);
    float bufferRMS = mAudioBuffer->getRMSLevel(0, 0, mAudioBuffer->getNumSamples());
    DBG("buffer RMS in AudioMod.cpp: " + std::to_string(bufferRMS));
    DBG("mAudioBuffer's memory address: " + std::to_string((long)mAudioBuffer.get()));

    model->process(mAudioBuffer.get(), sourceSampleRate, dawSampleRate);

    // connect the modified buffer to the source

    mIsModified = true;
    mThumbCreated = false;
  }
}

juce::AudioBuffer<float> *AudioModification::getModifiedAudioBuffer() {
  return mAudioBuffer.get();
}

bool AudioModification::getIsModified() const { return mIsModified; }
