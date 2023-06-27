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
 * @brief Implemented audio processor class.
 * @author JUCE, aldo aguilar, hugo flores garcia
 */

#pragma once

#include "AudioProcessorImpl.h"
#include "ProcessorEditor.h"
class TensorJuceAudioProcessor : public ARADemoPluginAudioProcessorImpl {
public:
  bool hasEditor() const override { return true; }
  AudioProcessorEditor *createEditor() override {
    return new TensorJuceProcessorEditor(
        *this, dynamic_cast<EditorRenderer *>(getEditorRenderer()));
  }
};
