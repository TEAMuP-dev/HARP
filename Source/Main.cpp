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
 * @brief Entry point, provide audio processor and ara document factory of
 * plugin.
 * @author JUCE, hugo flores garcia, aldo aguilar
 */

#include <JuceHeader.h>

#include "ARA/AudioProcessor.h"
#include "ARA/DocumentControllerSpecialisation.h"

//==============================================================================
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new HARPAudioProcessor();
}

#if JucePlugin_Enable_ARA
const ARA::ARAFactory *JUCE_CALLTYPE createARAFactory() {
  return juce::ARADocumentControllerSpecialisation::createARAFactory<
      HARPDocumentControllerSpecialisation>();
}
#endif
