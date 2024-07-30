//
//  NoteGridControlPanel.hpp
//  PianoRollEditor - App
//
//  Created by Samuel Hunt on 03/02/2020.
//

#pragma once

#ifndef NoteGridControlPanel_hpp
#define NoteGridControlPanel_hpp

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
#include "NoteGridComponent.hpp"
#include "NoteGridStyleSheet.hpp"

using namespace juce;

class NoteGridControlPanel : public Component {
public:
    NoteGridControlPanel (NoteGridComponent & component, NoteGridStyleSheet & styleSheet);
    ~NoteGridControlPanel ();
    
    void resized ();
    void paint (Graphics & g);
    
    void renderSequence ();
    std::function<void(int pixelsPerBar, int noteHeight)> configureGrid;
    
    void setQuantisation (PRE::eQuantisationValue value);
private:
    
    NoteGridComponent & noteGrid;
    NoteGridStyleSheet & styleSheet;
    
    Slider noteCompHeight, pixelsPerBar;
    
    TextButton render;
    ToggleButton drawMIDINotes, drawMIDIText, drawVelocity;
    
    ComboBox quantisationVlaue;
};

#endif /* NoteGridControlPanel_hpp */
