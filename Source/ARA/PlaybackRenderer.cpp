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
                                   ProcessingLockInterface &lockInterfaceIn,
                                   HARPDocumentControllerSpecialisation& harpDCS)
    : ARAPlaybackRenderer(dc), lockInterface(lockInterfaceIn), harpDCS(harpDCS) {
    }

// destructor
PlaybackRenderer::~PlaybackRenderer() {
  harpDCS.cleanDeletedPlaybackRenderers(this);
}

void PlaybackRenderer::prepareToPlay(double sampleRateIn,
                                     int maximumSamplesPerBlockIn,
                                     int numChannelsIn,
                                     AudioProcessor::ProcessingPrecision,
                                     AlwaysNonRealtime alwaysNonRealtime) {
  // DBG("PlaybackRenderer::prepareToPlay");
  numChannels = numChannelsIn;
  dawSampleRate = sampleRateIn;
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
            jmax(4 * maximumSamplesPerBlock, roundToInt(2.0 * dawSampleRate));

        readerSource = std::make_unique<juce::AudioFormatReaderSource>(
            new BufferingAudioReader(new ARAAudioSourceReader(audioSource),
                                     *sharedTimesliceThread, readAheadSize),
            true);
      }

      auto resamplingSource = std::make_unique<ResamplingAudioSource>(
          readerSource.get(), false, numChannels);

      resamplingSource->setResamplingRatio(audioSource->getSampleRate() / dawSampleRate
                                           );

      readerSource->prepareToPlay(maximumSamplesPerBlock, dawSampleRate);
      resamplingSource->prepareToPlay(maximumSamplesPerBlock, dawSampleRate);

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
// void PlaybackRenderer::didAddPlaybackRegion (PlaybackRegion* playbackRegion) {
//   DBG("PlaybackRenderer::didAddPlaybackRegion");
//   auto audioSource = playbackRegion->getAudioModification()->getAudioSource();

//   DBG("PlaybackRenderer::didAddPlaybackRegion audio source is " << audioSource->getName());
// }

bool PlaybackRenderer::processBlock(
    AudioBuffer<float> &buffer, AudioProcessor::Realtime realtime,
    const AudioPlayHead::PositionInfo &positionInfo) noexcept {
  const auto lock = lockInterface.getProcessingLock();
  ignoreUnused(realtime);
  if (!lock.isLocked()) {
    DBG("PlaybackRenderer::processBlock could not acquire processing lock");
    return true;
  }

  // all the sample-related variable names that end in assr are in the audio source sample rate
  // all other sample-related variables are in DAW time (dawSampleRate)
  const auto numSamples = buffer.getNumSamples();
  jassert(numSamples <= maximumSamplesPerBlock);
  jassert(numChannels == buffer.getNumChannels());
  // jassert (realtime == AudioProcessor::Realtime::no ||
  // useBufferedAudioSourceReader); TODO: bring me back?

  // time in samples based on the DAW's sample rate
  const auto timeInSamples = positionInfo.getTimeInSamples().orFallback(0);
  const auto isPlaying = positionInfo.getIsPlaying();
  // DBG numSamples and timeInSamples
  DBG("PlaybackRenderer::processBlock numSamples: " << numSamples
                                                    << ", timeInSamples: "
                                                    << timeInSamples);

  bool success = true;
  bool didRenderAnyRegion = false;

  if (isPlaying) {
    const auto blockRange =
        Range<int64>::withStartAndLength(timeInSamples, numSamples);
    // const auto blockRange =
    //     SampleRange::withStartAndLength(timeInSamples, numSamples);
    DBG("PlaybackRenderer::processBlock blockRange: " << blockRange.getStart()
                                                      << " "
                                                      << blockRange.getEnd());
    for (const auto &playbackRegion : getPlaybackRegions()) {
      auto sourceSampleRate = playbackRegion->getAudioModification()->getAudioSource()->getSampleRate();
      // DBG("PlaybackRenderer::processBlock evaluating playback region: "
      //     << playbackRegion->getRegionSequence()->getName() << " "
      //     << playbackRegion->getRegionSequence()->getDocument()->getName());
      // Evaluate region borders in song time, calculate sample range to render
      // in song time. Note that this example does not use head- or tailtime, so
      // the includeHeadAndTail parameter is set to false here - this might need
      // to be adjusted in actual plug-ins.

      // playbackRegion sample range in DAW time
      const auto playbackSampleRange = playbackRegion->getSampleRange(
          dawSampleRate, ARAPlaybackRegion::IncludeHeadAndTail::no);
      const auto playbackSampleRange_assr = playbackRegion->getSampleRange(
          sourceSampleRate, ARAPlaybackRegion::IncludeHeadAndTail::no);

      DBG("PlaybackRenderer::processBlock playbackSampleRange_assr: " << playbackSampleRange_assr.getStart() << " " << playbackSampleRange_assr.getEnd());
      DBG("PlaybackRenderer::processBlock playbackSampleRange: " <<
              playbackSampleRange.getStart() << " " << playbackSampleRange.getEnd());

      // blockRange may start before the playbackSampleRange, so we make sure
      // the renderRange starts at the same time as the playbackSampleRange
      auto renderRange = blockRange.getIntersectionWith(playbackSampleRange);
      DBG("PlaybackRenderer::processBlock renderRange: " <<
              renderRange.getStart() << " " << renderRange.getEnd());

      if (renderRange.isEmpty()) {
        DBG("PlaybackRenderer::processBlock render range is empty wrt to "
            "playback range");
        continue;
      }

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


      // Evaluate region borders in modification/source time and calculate
      // offset between song and source samples, then clip song samples
      // accordingly (if an actual plug-in supports time stretching, this must
      // be taken into account here).
      Range<int64> modificationSampleRange_assr{ // samples on source original sample rate (i.e 48k)
          playbackRegion->getStartInAudioModificationSamples(),
          playbackRegion->getEndInAudioModificationSamples()};
      Range<int64> modificationSampleRange{
          playbackRegion->getStartInPlaybackSamples(dawSampleRate),
          playbackRegion->getEndInPlaybackSamples(dawSampleRate)};

      const auto modificationSampleOffset_old =
          modificationSampleRange_assr.getStart() - roundToInt(playbackSampleRange.getStart() * resamplingSource->getResamplingRatio()); // playbackSampleRange is in DAW time
      const auto modificationSampleOffset_assr =
          modificationSampleRange_assr.getStart() - playbackSampleRange_assr.getStart();
      jassert(modificationSampleOffset_assr == modificationSampleOffset_old);
      DBG("PlaybackRenderer::processBlock modificationSampleRange_assr: " <<
              modificationSampleRange_assr.getStart() << " " << modificationSampleRange_assr.getEnd());
      DBG("PlaybackRenderer::processBlock modificationSampleRange: " <<
              modificationSampleRange.getStart() << " " << modificationSampleRange.getEnd());
      DBG("PlaybackRenderer::processBlock modificationSampleOffset_assr: " <<
              modificationSampleOffset_assr);

      renderRange = renderRange.getIntersectionWith( // renderRange.getStart() always same as playbackSampleRange.getStart() (unless the DAW doesn't work correctly (i.e. timeInSamples is larger than the playbackRegion's start time))
          modificationSampleRange.movedToStartAt( // so the only reason to do this intersection is to limit/cut the end of the renderRange based on the end of the modificationSampleRange_assr
              playbackSampleRange.getStart())); // TODO:  we should be using the sampleRateRation multiplier to get the modificationSampleRange_assr in DAW time
      DBG("PlaybackRenderer::processBlock renderRange after intersecting with mod: " <<
              renderRange.getStart() << " " << renderRange.getEnd());
      if (renderRange.isEmpty()) {
        DBG("PlaybackRenderer::processBlock render range is empty wrt to "
            "modification range");
        continue;
      }
      // !
      // -----------------------------------------------------------------------------------------



      // calculate buffer offsets

      // Calculate buffer offsets.
      const int numSamplesToRead = (int)renderRange.getLength();
      const int startInBuffer =
          (int)(renderRange.getStart() - blockRange.getStart());
      auto startInSource = roundToInt(renderRange.getStart() * resamplingSource->getResamplingRatio()) + modificationSampleOffset_assr;
      auto startInSourceDawTime = renderRange.getStart() + roundToInt(modificationSampleOffset_assr / resamplingSource->getResamplingRatio());
      DBG("PlaybackRenderer::processBlock numSamplesToRead in DAW time: " << numSamplesToRead);
      DBG("PlaybackRenderer::processBlock numSamplesToRead in source time: " << numSamplesToRead * resamplingSource->getResamplingRatio());
      DBG("PlaybackRenderer::processBlock startInSource: " << startInSource);
      DBG("PlaybackRenderer::processBlock startInBuffer: " << startInBuffer);


      // positionableSource->setNextReadPosition(roundToInt(startInSource * resamplingSource->getResamplingRatio()));
      // positionableSource->setNextReadPosition(roundToInt(startInSource + numSamplesToRead * resamplingSource->getResamplingRatio()));
      positionableSource->setNextReadPosition(startInSource );

      // DBG("PlaybackRenderer::processBlock setting next read position to " << roundToInt(startInSource * resamplingSource->getResamplingRatio()) );
      // DBG("PlaybackRenderer::processBlock setting next read position to " << roundToInt(startInSource + numSamplesToRead * resamplingSource->getResamplingRatio()) );
      DBG("PlaybackRenderer::processBlock setting next read position to " << startInSource);
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
                                static_cast<int>(startInSourceDawTime),
                                numSamplesToRead);
        }

        else if (modBuffer->getNumChannels() == 1) {
          for (int c = 0; c < numChannels; ++c)
            readBuffer.copyFrom(c, 0, *modBuffer, 0,
                                static_cast<int>(startInSourceDawTime),
                                numSamplesToRead);
        }

      } else { // buffer isn't ready, read from original audio source
        DBG("reading " << numSamplesToRead << " DAW time samples from " << roundToInt(startInSource * resamplingSource->getResamplingRatio())
                       << " into " << startInBuffer << " sample position in DAW buffer");
        resamplingSource->getNextAudioBlock(
            juce::AudioSourceChannelInfo(readBuffer));

        // resamplingSource->getNextAudioBlock(
        //     juce::AudioSourceChannelInfo(&readBuffer, startInBuffer, numSamplesToRead));

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

void PlaybackRenderer::executeProcess(std::shared_ptr<WebWave2Wave> model) {
  DBG("PlaybackRenderer::executeProcess executing process");

  auto callback = [this, model](juce::ARAPlaybackRegion *playbackRegion) -> bool {
      try {
          auto modification = playbackRegion->getAudioModification<AudioModification>();
          std::cout << "PlaybackRenderer::processing playbackRegion "
                    << modification->getSourceName() << std::endl;
          modification->process(model, dawSampleRate);
      } catch (const std::runtime_error& e) {
          juce::AlertWindow::showMessageBoxAsync(
              juce::AlertWindow::WarningIcon,
              "Processing Error",
              juce::String("An error occurred while processing: ") + e.what()
          );
          return false;  // Return false to indicate a failure (if necessary)
      }
      return true;
  };

  forEachPlaybackRegion(callback);
}

template <typename Callback>
void PlaybackRenderer::forEachPlaybackRegion(Callback &&cb) {
  for (const auto &playbackRegion : getPlaybackRegions()) {
    PlaybackRegion& region = static_cast<PlaybackRegion&>(*playbackRegion);
    if (region.isSelected()){
        auto modification =
            playbackRegion->getAudioModification<AudioModification>();
        DBG("PlaybackRenderer::forEachPlaybackRegion processing playbackRegion "
            << modification->getSourceName());
        if (!cb(playbackRegion))
        return;
    }
  }
  // NOTE : PlaybackRenderer doesn't get access to the regionSequences
  // for (const auto &regionSequence : getRegionSequences())
  //   for (const auto &playbackRegion : regionSequence->getPlaybackRegions())
  //     if (!cb(playbackRegion))
  //       return;
  // Question : does callback get called 2x for each region?
}
