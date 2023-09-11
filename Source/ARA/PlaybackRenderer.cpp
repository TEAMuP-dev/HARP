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
 * @brief Implementation of the ARA Playback Renderer.
 * This class serves samples back to the DAW for playback, and handles mixing
 * across tracks. We use this class to serve samples that have been processed
 * from a deeplearning model. When the host requests samples, we view which
 * playback region the playhead is located on, retrive the audio modification
 * for that playback region, and read the samples from the audio modifications
 * modified/processed audio buffered.
 * @author JUCE, aldo aguilar, hugo flores garcia
 */

/**
 * @file EditorRenderer.cpp
 * @brief Implements the EditorRenderer class.
 */

#include "PlaybackRenderer.h"

PlaybackRenderer::PlaybackRenderer(ARA::PlugIn::DocumentController *dc,
                                   ProcessingLockInterface &lockInterfaceIn)
    : ARAPlaybackRenderer(dc), lockInterface(lockInterfaceIn) {}

// destructor 
PlaybackRenderer::~PlaybackRenderer() {
  int aa = 42;
}

void PlaybackRenderer::prepareToPlay(double sampleRateIn,
                                     int maximumSamplesPerBlockIn,
                                     int numChannelsIn,
                                     AudioProcessor::ProcessingPrecision,
                                     AlwaysNonRealtime alwaysNonRealtime) {
  // DBG("PlaybackRenderer::prepareToPlay");
  numChannels = numChannelsIn;
  sampleRate = sampleRateIn;
  maximumSamplesPerBlock = maximumSamplesPerBlockIn;

  // DBG("PlaybackRenderer::prepareToPlay - numChannels: " << numChannels << ",
  // sampleRate: " << sampleRate << ", maximumSamplesPerBlock: " <<
  // maximumSamplesPerBlock);

  bool useBufferedAudioSourceReader =
      alwaysNonRealtime == AlwaysNonRealtime::no;

  // DBG("PlaybackRenderer::prepareToPlay using buffered audio source reader: "
  // << (int)useBufferedAudioSourceReader);

  for (const auto playbackRegion : getPlaybackRegions()) {
    auto audioSource = playbackRegion->getAudioModification()->getAudioSource();

    // DBG("PlaybackRenderer::prepareToPlay audio source is " <<
    // audioSource->getName());

    if (resamplingSources.find(audioSource) == resamplingSources.end()) {

      std::unique_ptr<juce::AudioFormatReaderSource> readerSource{nullptr};

      if (!useBufferedAudioSourceReader) {
        readerSource = std::make_unique<juce::AudioFormatReaderSource>(
            new ARAAudioSourceReader(audioSource), true);

      } else {
        const auto readAheadSize =
            jmax(4 * maximumSamplesPerBlock, roundToInt(2.0 * sampleRate));

        readerSource = std::make_unique<juce::AudioFormatReaderSource>(
            new BufferingAudioReader(new ARAAudioSourceReader(audioSource),
                                     *sharedTimesliceThread, readAheadSize),
            true);
      }

      auto resamplingSource = std::make_unique<ResamplingAudioSource>(
          readerSource.get(), false, numChannels);

      resamplingSource->setResamplingRatio(sampleRate /
                                           audioSource->getSampleRate());

      readerSource->prepareToPlay(maximumSamplesPerBlock, sampleRate);
      resamplingSource->prepareToPlay(maximumSamplesPerBlock, sampleRate);

      positionableSources.emplace(audioSource, std::move(readerSource));
      resamplingSources.emplace(audioSource, std::move(resamplingSource));
    }
  }
}

void PlaybackRenderer::releaseResources() {
  // DBG("PlaybackRenderer::releaseResources releasing resources");
  resamplingSources.clear();
  positionableSources.clear();

  tempBuffer.reset();
}

bool PlaybackRenderer::processBlock(
    AudioBuffer<float> &buffer, AudioProcessor::Realtime realtime,
    const AudioPlayHead::PositionInfo &positionInfo) noexcept {
  const auto lock = lockInterface.getProcessingLock();
  ignoreUnused(realtime);
  if (!lock.isLocked()) {
    DBG("PlaybackRenderer::processBlock could not acquire processing lock");
    return true;
  }

  const auto numSamples = buffer.getNumSamples();
  jassert(numSamples <= maximumSamplesPerBlock);
  jassert(numChannels == buffer.getNumChannels());
  // jassert (realtime == AudioProcessor::Realtime::no ||
  // useBufferedAudioSourceReader); TODO: bring me back?
  const auto timeInSamples = positionInfo.getTimeInSamples().orFallback(0);
  const auto isPlaying = positionInfo.getIsPlaying();

  bool success = true;
  bool didRenderAnyRegion = false;

  if (isPlaying) {
    const auto blockRange =
        Range<int64>::withStartAndLength(timeInSamples, numSamples);

    for (const auto &playbackRegion : getPlaybackRegions()) {
      DBG("PlaybackRenderer::processBlock evaluating playback region: "
          << playbackRegion->getRegionSequence()->getName() << " "
          << playbackRegion->getRegionSequence()->getDocument()->getName());
      // Evaluate region borders in song time, calculate sample range to render
      // in song time. Note that this example does not use head- or tailtime, so
      // the includeHeadAndTail parameter is set to false here - this might need
      // to be adjusted in actual plug-ins.
      const auto playbackSampleRange = playbackRegion->getSampleRange(
          sampleRate, ARAPlaybackRegion::IncludeHeadAndTail::no);
      auto renderRange = blockRange.getIntersectionWith(playbackSampleRange);

      if (renderRange.isEmpty()) {
        DBG("PlaybackRenderer::processBlock render range is empty wrt to "
            "playback range");
        continue;
      }
      // Evaluate region borders in modification/source time and calculate
      // offset between song and source samples, then clip song samples
      // accordingly (if an actual plug-in supports time stretching, this must
      // be taken into account here).
      Range<int64> modificationSampleRange{
          playbackRegion->getStartInAudioModificationSamples(),
          playbackRegion->getEndInAudioModificationSamples()};
      const auto modificationSampleOffset =
          modificationSampleRange.getStart() - playbackSampleRange.getStart();

      renderRange = renderRange.getIntersectionWith(
          modificationSampleRange.movedToStartAt(
              playbackSampleRange.getStart()));

      if (renderRange.isEmpty()) {
        DBG("PlaybackRenderer::processBlock render range is empty wrt to "
            "modification range");
        continue;
      }
      // !
      // -----------------------------------------------------------------------------------------

      // find our resampled source
      const auto resamplingSourceIt = resamplingSources.find(
          playbackRegion->getAudioModification()->getAudioSource());
      auto &resamplingSource = resamplingSourceIt->second;

      const auto positionableSourceIt = positionableSources.find(
          playbackRegion->getAudioModification()->getAudioSource());
      auto &positionableSource = positionableSourceIt->second;

      // TODO: the buffering audio reader should have a time out. dont' think it
      // currently has one if we're in realtime mode, timeout should be 0. else,
      // can be 100 ms

      // calculate buffer offsets

      // Calculate buffer offsets.
      const int numSamplesToRead = (int)renderRange.getLength();
      const int startInBuffer =
          (int)(renderRange.getStart() - blockRange.getStart());
      auto startInSource = renderRange.getStart() + modificationSampleOffset;

      positionableSource->setNextReadPosition(startInSource);

      // Read samples:
      // first region can write directly into output, later regions need to use
      // local buffer.
      auto &readBuffer = (didRenderAnyRegion) ? *tempBuffer : buffer;

      // apply the modified buffer
      auto *modBuffer =
          playbackRegion->getAudioModification<AudioModification>()
              ->getModifiedAudioBuffer();
      if (modBuffer != nullptr &&
          playbackRegion->getAudioModification<AudioModification>()
              ->getIsModified()) {
        jassert(numSamplesToRead <= modBuffer->getNumSamples());
        // we could handle more cases with channel mismatches better
        if (modBuffer->getNumChannels() == numChannels) {
          for (int c = 0; c < numChannels; ++c)
            readBuffer.copyFrom(c, 0, *modBuffer, c,
                                static_cast<int>(startInSource),
                                numSamplesToRead);
        }

        else if (modBuffer->getNumChannels() == 1) {
          for (int c = 0; c < numChannels; ++c)
            readBuffer.copyFrom(c, 0, *modBuffer, 0,
                                static_cast<int>(startInSource),
                                numSamplesToRead);
        }

      } else { // buffer isn't ready, read from original audio source
        DBG("reading " << numSamplesToRead << " samples from " << startInSource
                       << " into " << startInBuffer);

        resamplingSource->getNextAudioBlock(
            juce::AudioSourceChannelInfo(readBuffer));

        //     if (! reader.get()->read (&readBuffer, startInBuffer,
        //     numSamplesToRead, startInSource, true, true))
        //     {
        //         DBG("reader failed to read");
        //         success = false;
        //         continue;
        //     }
      }

      // Mix output of all regions
      if (didRenderAnyRegion) {
        // Mix local buffer into the output buffer.
        for (int c = 0; c < numChannels; ++c)
          buffer.addFrom(c, startInBuffer, *tempBuffer, c, startInBuffer,
                         numSamplesToRead);
      } else {
        // Clear any excess at start or end of the region.
        if (startInBuffer != 0)
          buffer.clear(0, startInBuffer);

        const int endInBuffer = startInBuffer + numSamplesToRead;
        const int remainingSamples = numSamples - endInBuffer;

        if (remainingSamples != 0)
          buffer.clear(endInBuffer, remainingSamples);

        didRenderAnyRegion = true;
      }
    }
  }

  // If no playback or no region did intersect, clear buffer now.
  if (!didRenderAnyRegion) {
    DBG("no region did intersect or no playback");
    buffer.clear();
  }
  return success;
}

void PlaybackRenderer::executeProcess(std::map<std::string, std::any> &params) {
  DBG("PlaybackRenderer::executeProcess executing process");

  auto callback = [&params](juce::ARAPlaybackRegion *playbackRegion) -> bool {
    auto modification =
        playbackRegion->getAudioModification<AudioModification>();
    std::cout << "PlaybackRenderer::processing playbackRegion "
              << modification->getSourceName() << std::endl;
    modification->process(params);
    return true;
  };

  forEachPlaybackRegion(callback);
}

template <typename Callback>
void PlaybackRenderer::forEachPlaybackRegion(Callback &&cb) {
  for (const auto &playbackRegion : getPlaybackRegions())
    if (!cb(playbackRegion))
      return;

  // NOTE : PlaybackRenderer doesn't get access to the regionSequences
  // for (const auto &regionSequence : getRegionSequences())
  //   for (const auto &playbackRegion : regionSequence->getPlaybackRegions())
  //     if (!cb(playbackRegion))
  //       return;
  // Question : does callback get called 2x for each region?
}