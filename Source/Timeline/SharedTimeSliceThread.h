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
 * @brief This class handles is a wrapper for the juceTimeSliceThread
 * more information can be found on the JUCE documentation
 * https://docs.juce.com/master/classTimeSliceThread.html
 * @author hugo flores garcia, aldo aguilar
 */

#pragma once

#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_audio_formats/juce_audio_formats.h"

class SharedTimeSliceThread : public juce::TimeSliceThread {
public:
  SharedTimeSliceThread()
      : TimeSliceThread(juce::String(JucePlugin_Name) +
                        " ARA Sample Reading Thread") {
    startThread(Priority::high); // Above default priority so playback is
                                 // fluent, but below realtime
  }
};
