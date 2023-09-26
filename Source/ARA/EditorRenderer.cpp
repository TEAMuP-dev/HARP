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

/**
 * @file EditorRenderer.cpp
 * @brief Implements the EditorRenderer class.
 */

#include "EditorRenderer.h"



EditorRenderer::EditorRenderer(
    ARA::PlugIn::DocumentController *documentController,
    const PreviewState *previewStateIn,
    ProcessingLockInterface &lockInterfaceIn)
    : ARAEditorRenderer(documentController), lockInterface(lockInterfaceIn),
      previewState(previewStateIn), asyncConfigCallback{
                                        [this] { configure(); }} {
  jassert(previewState != nullptr);
}

// EditorRenderer::~EditorRenderer() {
//   for (const auto &rs : regionSequences)
//     rs->removeListener(this);
// }

void EditorRenderer::didAddPlaybackRegionToRegionSequence(juce::ARARegionSequence *,
                                                          juce::ARAPlaybackRegion *) {
  asyncConfigCallback.startConfigure();
}

void EditorRenderer::didAddRegionSequence(
    ARA::PlugIn::RegionSequence *rs) noexcept {
  auto *sequence = static_cast<juce::ARARegionSequence *>(rs);
  sequence->addListener(this);
  regionSequences.insert(sequence);
  asyncConfigCallback.startConfigure();
}

void EditorRenderer::didAddPlaybackRegion(
    ARA::PlugIn::PlaybackRegion *) noexcept {
  asyncConfigCallback.startConfigure();
}


template <typename Callback>
void EditorRenderer::forEachPlaybackRegion(Callback &&cb) {
  for (const auto &playbackRegion : getPlaybackRegions())
    if (!cb(playbackRegion))
      return;

  for (const auto &regionSequence : getRegionSequences())
    for (const auto &playbackRegion : regionSequence->getPlaybackRegions())
      if (!cb(playbackRegion))
        return;
}

void EditorRenderer::prepareToPlay(double sampleRateIn,
                                   int maximumExpectedSamplesPerBlock,
                                   int numChannels,
                                   juce::AudioProcessor::ProcessingPrecision,
                                   AlwaysNonRealtime alwaysNonRealtime) {
  sampleRate = sampleRateIn;
  previewBuffer = std::make_unique<juce::AudioBuffer<float>>(numChannels,
                                                       (int)(4 * sampleRateIn));

  ignoreUnused(maximumExpectedSamplesPerBlock, alwaysNonRealtime);
}

void EditorRenderer::releaseResources() { audioSourceReaders.clear(); }

void EditorRenderer::reset() { previewBuffer->clear(); }

bool EditorRenderer::processBlock(
    juce::AudioBuffer<float> &buffer, juce::AudioProcessor::Realtime realtime,
    const juce::AudioPlayHead::PositionInfo &positionInfo) noexcept {
    
  ignoreUnused(buffer, realtime, positionInfo);
  ignoreUnused(realtime);

}

void EditorRenderer::configure() {
  forEachPlaybackRegion([this, maximumExpectedSamplesPerBlock =
                                   1000](const auto &playbackRegion) {
    const auto audioSource =
        playbackRegion->getAudioModification()->getAudioSource();

    if (audioSourceReaders.find(audioSource) == audioSourceReaders.end()) {
      audioSourceReaders[audioSource] = std::make_unique<juce::BufferingAudioReader>(
          new juce::ARAAudioSourceReader(
              playbackRegion->getAudioModification()->getAudioSource()),
          *timeSliceThread,
          std::max(4 * maximumExpectedSamplesPerBlock, (int)sampleRate));
    }
    
    // model = playbackRegion->template getAudioModification<AudioModification>()->getModel();

    return true;
  });
}
