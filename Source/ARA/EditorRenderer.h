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
 * @brief Implementation of the ARA Editor Renderer.
 * The ARA Editor Renderer is used to handle audio feedback from the plugin.
 * This allows us to playback samples from the host without the host in playback
 * mode. Editor Renderer will handle any audio rendering that is done soley in
 * the plugin.
 * @author JUCE, aldo aguilar, hugo flores garcia, xribene
 */

#pragma once

#include <any>
#include <map>

#include <ARA_Library/PlugIn/ARAPlug.h>
#include <ARA_Library/Utilities/ARAPitchInterpretation.h>
#include <ARA_Library/Utilities/ARATimelineConversion.h>

#include "../Timeline/SharedTimeSliceThread.h"
#include "../Util/AsyncConfigurationCallback.h"
#include "../Util/Looper.h"
#include "../Util/PreviewState.h"
#include "../Util/ProcessingLockInterface.h"
#include "AudioModification.h"
// #include "ProcessorEditor.h"
// using MyType = void (TensorJuceProcessorEditor::*)(std::string);
using std::unique_ptr;

// TODO: Documentation for readPlaybackRangeIntoBuffer.
std::optional<juce::Range<juce::int64>> readPlaybackRangeIntoBuffer(
    juce::Range<double> playbackRange, const juce::ARAPlaybackRegion *playbackRegion,
    juce::AudioBuffer<float> &buffer,
    const std::function<juce::AudioFormatReader *(juce::ARAAudioSource *)> &getReader);

/**
 * @class EditorRenderer
 * @brief TODO: Write brief class description.
 */
class EditorRenderer : public juce::ARAEditorRenderer,
                       private juce::ARARegionSequence::Listener {
public:
  EditorRenderer(ARA::PlugIn::DocumentController *documentController,
                 const PreviewState *previewStateIn,
                 ProcessingLockInterface &lockInterfaceIn);
  ~EditorRenderer() override;

  void didAddPlaybackRegionToRegionSequence(juce::ARARegionSequence *,
                                            juce::ARAPlaybackRegion *) override;
  void didAddRegionSequence(ARA::PlugIn::RegionSequence *rs) noexcept override;
  void didAddPlaybackRegion(ARA::PlugIn::PlaybackRegion *) noexcept override;


  void executeProcess(std::map<std::string, std::any> &params);
  // void executeLoad(std::map<std::string, std::any> &params, juce::ChangeListener* listener);

  template <typename Callback> void forEachPlaybackRegion(Callback &&cb);

  void prepareToPlay(double sampleRateIn, int maximumExpectedSamplesPerBlock,
                     int numChannels, juce::AudioProcessor::ProcessingPrecision,
                     AlwaysNonRealtime alwaysNonRealtime) override;

  void releaseResources() override;
  void reset() override;

  bool processBlock(
      juce::AudioBuffer<float> &buffer, juce::AudioProcessor::Realtime realtime,
      const juce::AudioPlayHead::PositionInfo &positionInfo) noexcept override;

  using ARAEditorRenderer::processBlock;

  std::shared_ptr<TorchWave2Wave> getModel() { 
    return model;
  }

private:
  void configure();

  std::shared_ptr<TorchWave2Wave> model {nullptr};

  ProcessingLockInterface &lockInterface;
  const PreviewState *previewState = nullptr;
  AsyncConfigurationCallback asyncConfigCallback;
  double lastPreviewTime = 0.0;
  juce::ARAPlaybackRegion *lastPlaybackRegion = nullptr;
  bool lastPreviewDimmed = false;
  bool wasPreviewing = false;
  unique_ptr<juce::AudioBuffer<float>> previewBuffer;
  Looper previewLooper;

  double sampleRate = 48000.0;
  juce::SharedResourcePointer<SharedTimeSliceThread> timeSliceThread;
  std::map<juce::ARAAudioSource *, unique_ptr<juce::BufferingAudioReader>>
      audioSourceReaders;

  std::set<juce::ARARegionSequence *> regionSequences;
};
