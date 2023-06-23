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
  * @brief Class used to cross fade audio sources.
  * @author JUCE, hugo flores garcia, aldo aguilar
  */

#pragma once

static void crossfade (const float* sourceA,
                       const float* sourceB,
                       float aProportionAtStart,
                       float aProportionAtFinish,
                       float* destinationBuffer,
                       int numSamples)
{
    AudioBuffer<float> destination { &destinationBuffer, 1, numSamples };
    destination.copyFromWithRamp (0, 0, sourceA, numSamples, aProportionAtStart, aProportionAtFinish);
    destination.addFromWithRamp (0, 0, sourceB, numSamples, 1.0f - aProportionAtStart, 1.0f - aProportionAtFinish);
}
