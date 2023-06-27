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
 * @brief The audio processing class for the ARA plugin.
 * @author JUCE, aldo aguilar, hugo flores garcia
 */
#pragma once

#include "AudioProcessorImpl.h"

void ARADemoPluginAudioProcessorImpl::prepareToPlay(double sampleRate,
                                                    int samplesPerBlock) {
  playHeadState.update(nullopt);
  prepareToPlayForARA(sampleRate, samplesPerBlock,
                      getMainBusNumOutputChannels(), getProcessingPrecision());
}

void ARADemoPluginAudioProcessorImpl::releaseResources() {
  playHeadState.update(nullopt);
  releaseResourcesForARA();
}

bool ARADemoPluginAudioProcessorImpl::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono() &&
      layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
    return false;

  return true;
}

void ARADemoPluginAudioProcessorImpl::processBlock(AudioBuffer<float> &buffer,
                                                   MidiBuffer &midiMessages) {
  ignoreUnused(midiMessages);

  ScopedNoDenormals noDenormals;

  auto *audioPlayHead = getPlayHead();
  playHeadState.update(audioPlayHead->getPosition());

  if (!processBlockForARA(buffer, isRealtime(), audioPlayHead))
    processBlockBypassed(buffer, midiMessages);
}

double ARADemoPluginAudioProcessorImpl::getTailLengthSeconds() const {
  double tail;
  if (getTailLengthSecondsForARA(tail))
    return tail;

  return 0.0;
}

juce::AudioProcessor::BusesProperties
ARADemoPluginAudioProcessorImpl::getBusesProperties() {
  return BusesProperties()
      .withInput("Input", AudioChannelSet::stereo(), true)
      .withOutput("Output", AudioChannelSet::stereo(), true);
}
