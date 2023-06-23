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
 * @brief Caches thumbnail representations of waveforms.
 * @author JUCE, hugo flores garcia, aldo aguilar
 */

#pragma once

#include <ARA_Library/Utilities/ARAPitchInterpretation.h>
#include <ARA_Library/Utilities/ARATimelineConversion.h>

#include "juce_audio_formats/juce_audio_formats.h"
#include "juce_audio_utils/juce_audio_utils.h"

#include "../ARA/AudioModification.h"

using namespace juce;

struct WaveformCache : private ARAAudioSource::Listener {
  WaveformCache() : thumbnailCache(20) {}

  ~WaveformCache() override {
    for (const auto &entry : thumbnails) {
      entry.first->removeListener(this);
    }
  }

  //==============================================================================
  void willDestroyAudioSource(ARAAudioSource *audioSource) override {
    removeAudioSource(audioSource);
  }

  AudioThumbnail &getOrCreateThumbnail(ARAAudioSource *audioSource,
                                       AudioModification *audioModification) {
    const auto iter = thumbnails.find(audioSource);

    if (iter != std::end(thumbnails))
      return *iter->second;

    auto thumb =
        std::make_unique<AudioThumbnail>(128, dummyManager, thumbnailCache);
    auto &result = *thumb;

    ++hash;
    if (audioModification->getIsModified())
      thumb->setSource(audioModification->getModifiedAudioBuffer(),
                       audioSource->getSampleRate(), hash);

    else
      thumb->setReader(new ARAAudioSourceReader(audioSource), hash);

    audioSource->addListener(this);
    thumbnails.emplace(audioSource, std::move(thumb));
    return result;
  }

private:
  void removeAudioSource(ARAAudioSource *audioSource) {
    audioSource->removeListener(this);
    thumbnails.erase(audioSource);
  }

  int64 hash = 0;
  AudioFormatManager dummyManager;
  AudioThumbnailCache thumbnailCache;
  std::map<ARAAudioSource *, unique_ptr<AudioThumbnail>> thumbnails;
};
