//
//  PNoteComponent.hpp
//  PianoRollEditor - App
//
//  Created by Samuel Hunt on 16/08/2019.
//

#pragma once

#ifndef PNoteComponent_hpp
#define PNoteComponent_hpp

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
#include "NoteModel.hpp"

using namespace juce;

class PNoteComponent :
public Component
{
public:

    enum eState {
        eNone,
        eSelected,
    };
    struct MultiEditState {
        int startWidth; //used when resizing the noteComponents length
        bool coordiantesDiffer; //sometimes the size of this component gets changed externally (for example on multi-resizing) set this flag to be true and at
        Rectangle<int> startingBounds;
    };

    PNoteComponent ();
    ~PNoteComponent ();

    void paint (Graphics & g);
    void resized ();
    void setCustomColour (Colour c);

    void setValues (NoteModel model);
    NoteModel getModel ();
    NoteModel * getModelPtr ();

    void setState (eState state);
    eState getState ();

//    void mouseDoubleClick (const MouseEvent&);

    int minWidth = 10;
    int startWidth; //used when resizing the noteComponents length
    int startX, startY;
    bool coordiantesDiffer; //sometimes the size of this component gets changed externally (for example on multi-resizing) set this flag to be true and at some point the internal model will get updated also
    bool isMultiDrag;

private:
    bool mouseOver, useCustomColour, resizeEnabled, velocityEnabled;
    int startVelocity;

    Colour customColour;
    NoteModel model;
    MouseCursor normal;
    eState state;
};

#endif /* NoteComponent_hpp */
