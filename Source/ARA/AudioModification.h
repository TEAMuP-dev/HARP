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
 * @brief Audio modification header started from the ARAPluginDemo.h file
 * provided by juce. This class now also manages offline processing.
 * @author JUCE, aldo aguilar, hugo flores garcia, xribene
 */

#pragma once

#include "../DeepLearning/WebModel.h"
#include <ARA_Library/PlugIn/ARAPlug.h>

using std::unique_ptr;

/**
 * @class AudioModification
 * @brief This class provides methods for manipulating audio modifications.
 *
 * The AudioModification class contains any modifications made to the audio.
 * We utilize the class to perform deeplearning processing on a playback region.
 * Each playback region has its own audio modification,
 * so we can retrive the modified audio from a playback region.
 */

class AudioModification : public juce::ARAAudioModification {
public:
  AudioModification(juce::ARAAudioSource *audioSource,
                    ARA::ARAAudioModificationHostRef hostRef,
                    const juce::ARAAudioModification *optionalModificationToClone);

  void setThumbCreated(bool created);
  bool isThumbCreated() const;
  std::string getSourceName();

  void process(std::shared_ptr<WebWave2Wave> model, double dawSampleRate);

  juce::AudioBuffer<float> *getModifiedAudioBuffer();
  bool getIsModified() const;

private:
  bool mIsModified = false;
  bool mThumbCreated = false;

  std::string mAudioSourceName;
  int mSampleRate;
  unique_ptr<juce::ARAAudioSourceReader> mAudioSourceReader{nullptr};
  unique_ptr<juce::AudioBuffer<float>> mAudioBuffer{nullptr};
};
