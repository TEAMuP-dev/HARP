//
//  KeyboardComponent.hpp
//  PianoRollEditor - App
//
//  Created by Samuel Hunt on 04/02/2020.
//
#pragma once

#ifndef KeyboardComponent_hpp
#define KeyboardComponent_hpp

// #include "PConstants.h"
// using namespace juce;

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_dsp/juce_dsp.h>
// #include <juce_events/timers/juce_Timer.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

using namespace juce;

class KeyboardComponent : public Component {
public:
    
    KeyboardComponent ();
    void paint (Graphics & g);
    
private:
    Array<int> blackPitches;

};

#endif /* KeyboardComponent_hpp */
