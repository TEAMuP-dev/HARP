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
#include "NoteGridStyleSheet.hpp"

using namespace juce;

class PNoteComponent :
public Component,
public ComponentDragger
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
    
    PNoteComponent (NoteGridStyleSheet & styleSheet);
    ~PNoteComponent ();
    
    void paint (Graphics & g);
    void resized ();
    void setCustomColour (Colour c);
    
    void setValues (NoteModel model);
    NoteModel getModel ();
    NoteModel * getModelPtr ();
    
    void setState (eState state);
    eState getState ();
    
    void mouseEnter (const MouseEvent&) override;
    void mouseExit  (const MouseEvent&) override;
    void mouseDown  (const MouseEvent&) override;
    void mouseUp    (const MouseEvent&) override;
    void mouseDrag  (const MouseEvent&) override;
    void mouseMove  (const MouseEvent&) override;
    
    
//    void mouseDoubleClick (const MouseEvent&);
    
    std::function<void(PNoteComponent*, const MouseEvent&)> onNoteSelect;
    std::function<void(PNoteComponent*)> onPositionMoved;
    std::function<void(PNoteComponent*, const MouseEvent&)> onDragging; //send the drag event to other components..
    std::function<void(PNoteComponent*, int)> onLegnthChange; //sends the difference as this is relative for all components.
    
    int minWidth = 10;
    int startWidth; //used when resizing the noteComponents length
    int startX, startY;
    bool coordiantesDiffer; //sometimes the size of this component gets changed externally (for example on multi-resizing) set this flag to be true and at some point the internal model will get updated also
    bool isMultiDrag;
    
private:
    NoteGridStyleSheet & styleSheet;
    ResizableEdgeComponent edgeResizer;
    
    bool mouseOver, useCustomColour, resizeEnabled, velocityEnabled;
    int startVelocity;
    
    Colour customColour;
    NoteModel model;
    MouseCursor normal;
    eState state;

};

#endif /* NoteComponent_hpp */
