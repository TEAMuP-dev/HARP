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

#pragma once

#include "DocumentControllerSpecialisation.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_audio_formats/juce_audio_formats.h"

#include <ARA_Library/PlugIn/ARAPlug.h>
#include <ARA_Library/Utilities/ARAPitchInterpretation.h>
#include <ARA_Library/Utilities/ARATimelineConversion.h>

#include "../Timeline/SharedTimeSliceThread.h"
#include "../Util/ProcessingLockInterface.h"
#include "AudioModification.h"
#include "PlaybackRegion.h"


using namespace juce;
using std::unique_ptr;

// class SampleRange : public Range<int64>
// {
// public:
//     using Range<int64>::Range; // Inherit constructors from the base class

//     // Constructor to create a ScalableRange from a Range
//     ScalableRange(const Range<int64>& range) noexcept : Range<int64>(range) {}
//     // Scale the range by a <double> factor
//     ScalableRange operator*(const double factor) const noexcept
//     {
//         return ScalableRange(roundToInt(this->getStart() * factor), 
//                             roundToInt(this->getEnd() * factor));
//     }
// };

/**
 * @class PlaybackRenderer
 * @brief Class responsible for rendering playback.
 *
 * This class inherits from the ARAPlaybackRenderer base class.
 */
class PlaybackRenderer : public ARAPlaybackRenderer {
public:
  /**
   * @brief Constructor for PlaybackRenderer class.
   *
   * @param dc DocumentController instance.
   * @param lockInterfaceIn Lock interface to use during processing.
   */
  PlaybackRenderer(ARA::PlugIn::DocumentController *dc,
                   ProcessingLockInterface &lockInterfaceIn,
                   HARPDocumentControllerSpecialisation &harpDCS);

  /**
   * @brief Destructor for PlaybackRenderer class.
   */
    ~PlaybackRenderer();

  /**
   * @brief Prepares for playback by initializing necessary parameters.
   *
   * @param sampleRateIn The sample rate to use for playback.
   * @param maximumSamplesPerBlockIn The maximum number of samples per block.
   * @param numChannelsIn The number of audio channels.
   * @param alwaysNonRealtime Flag indicating whether to always use non-realtime
   * mode.
   */
  void prepareToPlay(double sampleRateIn, int maximumSamplesPerBlockIn,
                     int numChannelsIn, AudioProcessor::ProcessingPrecision,
                     AlwaysNonRealtime alwaysNonRealtime) override;

  /**
   * @brief Releases all resources used by the PlaybackRenderer.
   */
  void releaseResources() override;
  // void didAddPlaybackRegion (ARA::PlugIn::PlaybackRegion* playbackRegion) noexcept override {}
  // void didAddPlaybackRegion(ARA::PlugIn::PlaybackRegion *) noexcept override;
  /**
   * @brief Processes an audio block for playback.
   *
   * @param buffer The audio buffer to process.
   * @param realtime The Realtime processing mode to use.
   * @param positionInfo Position information for the playback head.
   *
   * @return True if the process was successful, false otherwise.
   */
  bool processBlock(
      AudioBuffer<float> &buffer, AudioProcessor::Realtime realtime,
      const AudioPlayHead::PositionInfo &positionInfo) noexcept override;
  using ARAPlaybackRenderer::processBlock;

  /**
   * @brief Processes the each of the playback regions using the Neural Model
   * @param params The parameters from the UI to use for processing. 
   * @return void
   */
  void executeProcess(std::shared_ptr<WebWave2Wave> model);

  template <typename Callback> void forEachPlaybackRegion(Callback &&cb);

private:
  ProcessingLockInterface &lockInterface;
  HARPDocumentControllerSpecialisation &harpDCS;
  SharedResourcePointer<SharedTimeSliceThread> sharedTimesliceThread;
  std::map<ARAAudioSource *, unique_ptr<juce::ResamplingAudioSource>>
      resamplingSources;
  std::map<ARAAudioSource *, unique_ptr<AudioFormatReaderSource>>
      positionableSources;
  int numChannels = 2;
  double dawSampleRate;
  int maximumSamplesPerBlock = 128;
  unique_ptr<AudioBuffer<float>> tempBuffer;
};
