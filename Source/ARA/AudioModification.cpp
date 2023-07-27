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
 * @author JUCE, aldo aguilar, hugo flores garcia
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
    : ARAAudioModification(audioSource, hostRef, optionalModificationToClone) {

  DBG("AudioModification::created");
  DBG("AudioModification::the audio source is " << audioSource->getName());

  init(audioSource);
}

/**
 * @brief Initialization deep learning model for audio modification
 *
 * @param audioSource Pointer to the ARAAudioSource to perform processing on
 */
void AudioModification::init(juce::ARAAudioSource *audioSource) {
  DBG("loading model");

  // map<string, any> params {{"path",
  // "/Users/hugo/projects/plugin_sandbox/reduceamp.pt"}};
  map<string, any> params{{"url", string("http://127.0.0.1:7860")},
                          {"api_name", string("/api/predict/")}};
  if (!mModel.load(params)) { // change model here
    DBG("failed to load model");
  } else {
    DBG("model loaded");
  }

  DBG("AudioModification:: create reader for " << audioSource->getName());
  mAudioSourceReader = std::make_unique<juce::ARAAudioSourceReader>(audioSource);
  mSampleRate = audioSource->getSampleRate();
  mAudioSourceName = audioSource->getName();
}

bool AudioModification::isDimmed() const { return dimmed; }

void AudioModification::setDimmed(bool shouldDim) { dimmed = shouldDim; }

std::string AudioModification::getSourceName() { return mAudioSourceName; }

/**
 * @brief Process audio from audio source with deep learning effect
 *
 * @param params Map of parameters for learner
 */
void AudioModification::process(std::map<std::string, std::any> &params) {
  if (!mModel.ready()) {
    return;
  }

  if (!mAudioSourceReader->isValid())
    DBG("AudioModification:: invalid audio source reader");
  else {
    auto numChannels = mAudioSourceReader->numChannels;
    auto numSamples = mAudioSourceReader->lengthInSamples;
    auto sampleRate = mSampleRate;

    DBG("AudioModification:: audio source: "
        << mAudioSourceName << " channels: " << juce::String(numChannels)
        << " length in samples: " << juce::String(numSamples));

    mAudioBuffer.reset(new juce::AudioBuffer<float>(numChannels, numSamples));

    // reading into audio buffer
    mAudioSourceReader->read(mAudioBuffer.get(), 0,
                             static_cast<int>(numSamples), 0, true, true);
    mModel.process(mAudioBuffer.get(), sampleRate);

    // connect the modified buffer to the source

    mIsModified = true;
  }
}

juce::AudioBuffer<float> *AudioModification::getModifiedAudioBuffer() {
  return mAudioBuffer.get();
}

bool AudioModification::getIsModified() { return mIsModified; }
