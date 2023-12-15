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


struct WaveformCache : private juce::ARAAudioModification::Listener {
  WaveformCache() : thumbnailCache(20) {}

  ~WaveformCache() override {
    for (const auto &entry : thumbnails) {
      entry.first->removeListener(this);
    }
  }

  //==============================================================================
  void willDestroyAudioModification (juce::ARAAudioModification* audioModification) override {
    removeAudioModification(audioModification);
  }

  juce::AudioThumbnail &getOrCreateThumbnail(juce::ARAAudioSource *audioSource,
                                       AudioModification *audioModification) {
    const auto iter = thumbnails.find(audioModification);

    // if a thumbnail was found for this source,
    if (iter != std::end(thumbnails))
      if (audioModification->getIsModified()){
        // if the audio has been modified even once,
        // this flag will remain true forever.
        // That's why we'll check if a thumbnail has been
        // created for the latest processing step.
        if (audioModification->isThumbCreated()){
          // it means the current thumbnail is the updated one,
          // and we just need to return it.
          return *iter->second;
        }
        else{
          // It means there has been a processing step since
          // the last time the thumbnail was created, so we need
          // to update it.
          ++hash;
          iter->second->setSource(audioModification->getModifiedAudioBuffer(),
                              audioModification->getDawSampleRate(), hash);
          audioModification->setThumbCreated(true);
          return *iter->second;
        }
      }
      else{
        // if the source has never been modified then
        // we just return the thumbnail previously created.
        return *iter->second;
      }


    // if not found, create the thumbnail
    auto thumb =
        std::make_unique<juce::AudioThumbnail>(128, dummyManager, thumbnailCache);
    auto &result = *thumb;

    ++hash;
    if (audioModification->getIsModified())
      thumb->setSource(audioModification->getModifiedAudioBuffer(),
                      audioModification->getDawSampleRate(), hash);

    else
      thumb->setReader(new juce::ARAAudioSourceReader(audioSource), hash);

    audioModification->addListener(this);
    thumbnails.emplace(audioModification, std::move(thumb));
    audioModification->setThumbCreated(true);
    return result;
  }

private:
  void removeAudioModification(juce::ARAAudioModification *audioModification) {
    audioModification->removeListener(this);
    thumbnails.erase(audioModification);
  }

  juce::int64 hash = 0;
  juce::AudioFormatManager dummyManager;
  juce::AudioThumbnailCache thumbnailCache;
  std::map<juce::ARAAudioModification *, unique_ptr<juce::AudioThumbnail>> thumbnails;
};
