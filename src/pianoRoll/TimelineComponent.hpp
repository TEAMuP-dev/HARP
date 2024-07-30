//
//  TimelineComponent.hpp
//  PianoRollEditor - App
//
//  Created by Samuel Hunt on 04/02/2020.
//

#pragma once

#ifndef TimelineComponent_hpp
#define TimelineComponent_hpp

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

#include "PConstants.h"

using namespace juce;

/*
 Timelime component assumes 4/4
 todo: add support for other time signatures
 */
class TimelineComponent : public Component {
public:
    
    TimelineComponent ();
    //todo
    //TimelineComponent (const timeSig);
    
    void setup (const int barsToDraw, const int pixelsPerBar);
    void paint (Graphics & g);
    void resized ();
private:
    int barsToDraw;
    int pixelsPerBar;
};

#endif /* TimelineComponent_hpp */
