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
 * @brief Class used to loop sections of audio used by the
 * Editor Renderer.
 * @author JUCE, hugo flores garcia, aldo aguilar
 */

#pragma once

#include "Crossfade.h"

class Looper {
public:
  Looper() : inputBuffer(nullptr), pos(loopRange.getStart()) {}

  Looper(const juce::AudioBuffer<float> *buffer, juce::Range<juce::int64> range)
      : inputBuffer(buffer), loopRange(range), pos(range.getStart()) {}

  void writeInto(juce::AudioBuffer<float> &buffer) {
    if (loopRange.getLength() == 0) {
      buffer.clear();
      return;
    }

    const auto numChannelsToCopy =
        std::min(inputBuffer->getNumChannels(), buffer.getNumChannels());
    const auto actualCrossfadeLengthSamples = std::min(
        loopRange.getLength() / 2, (juce::int64)desiredCrossfadeLengthSamples);

    for (auto samplesCopied = 0; samplesCopied < buffer.getNumSamples();) {
      const auto [needsCrossfade, samplePosOfNextCrossfadeTransition] =
          [&]() -> std::pair<bool, juce::int64> {
        if (const auto endOfFadeIn =
                loopRange.getStart() + actualCrossfadeLengthSamples;
            pos < endOfFadeIn)
          return {true, endOfFadeIn};

        return {false, loopRange.getEnd() - actualCrossfadeLengthSamples};
      }();

      const auto samplesToNextCrossfadeTransition =
          samplePosOfNextCrossfadeTransition - pos;
      const auto numSamplesToCopy =
          std::min(buffer.getNumSamples() - samplesCopied,
                   (int)samplesToNextCrossfadeTransition);

      const auto getFadeInGainAtPos = [this,
                                       actualCrossfadeLengthSamples](auto p) {
        return juce::jmap((float)p, (float)loopRange.getStart(),
                    (float)loopRange.getStart() +
                        (float)actualCrossfadeLengthSamples - 1.0f,
                    0.0f, 1.0f);
      };

      for (int i = 0; i < numChannelsToCopy; ++i) {
        if (needsCrossfade) {
          const auto overlapStart = loopRange.getEnd() -
                                    actualCrossfadeLengthSamples +
                                    (pos - loopRange.getStart());

          crossfade(inputBuffer->getReadPointer(i, (int)pos),
                    inputBuffer->getReadPointer(i, (int)overlapStart),
                    getFadeInGainAtPos(pos),
                    getFadeInGainAtPos(pos + numSamplesToCopy),
                    buffer.getWritePointer(i, samplesCopied), numSamplesToCopy);
        } else {
          buffer.copyFrom(i, samplesCopied, *inputBuffer, i, (int)pos,
                          numSamplesToCopy);
        }
      }

      samplesCopied += numSamplesToCopy;
      pos += numSamplesToCopy;

      jassert(pos <= loopRange.getEnd() - actualCrossfadeLengthSamples);

      if (pos == loopRange.getEnd() - actualCrossfadeLengthSamples)
        pos = loopRange.getStart();
    }
  }

private:
  static constexpr int desiredCrossfadeLengthSamples = 50;

  const juce::AudioBuffer<float> *inputBuffer;
  juce::Range<juce::int64> loopRange;
  juce::int64 pos;
};
