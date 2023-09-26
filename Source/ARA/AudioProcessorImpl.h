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
 * @brief The audio processing header for the ARA plugin.
 * @author JUCE, aldo aguilar, hugo flores garcia
 */

#pragma once

#include "juce_audio_processors/juce_audio_processors.h"

#include "../Timeline/PlayheadPositionLabel.h"

/**
 * @class TensorJuceAudioProcessorImpl
 * @brief A class that implements the audio processing for the plugin.
 */
class TensorJuceAudioProcessorImpl : public AudioProcessor,
                                     public AudioProcessorARAExtension {
public:
  TensorJuceAudioProcessorImpl() : AudioProcessor(getBusesProperties()) {
  }
  ~TensorJuceAudioProcessorImpl() override = default;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;
  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
  void processBlock(AudioBuffer<float> &buffer,
                    MidiBuffer &midiMessages) override;

  using AudioProcessor::processBlock;

  const String getName() const override { return "tensor-juce"; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return true; }
  double getTailLengthSeconds() const override;

  int getNumPrograms() override { return 0; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const String getProgramName(int) override { return "None"; }
  void changeProgramName(int, const String &) override {}

  void getStateInformation(MemoryBlock &) override {}
  void setStateInformation(const void *, int) override {}

  PlayHeadState playHeadState;

private:
  static BusesProperties getBusesProperties();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TensorJuceAudioProcessorImpl)
};
