//
//  NoteGridStyleSheet.hpp
//  PianoRollEditor - App
//
//  Created by Samuel Hunt on 03/02/2020.
//

#ifndef NoteGridStyleSheet_hpp
#define NoteGridStyleSheet_hpp

#include "PConstants.h"

/*
 TODO extend this class so that it controls other elements of style, i.e. colours..
 */
class NoteGridStyleSheet {
public:
    /*
     We use a friend class to ensure a one-way only relationship. The control pannel can set member varialbes easily. Saves adding loads of getters/setters
     */
    friend class NoteGridControlPanel;
    
    NoteGridStyleSheet ();
    
    bool getDrawMIDINum ();
    bool getDrawMIDINoteStr ();
    bool getDrawVelocity ();
    
    bool disableEditing;
    
private:
    bool drawMIDINum, drawMIDINoteStr, drawVelocity;
    
    
};

#endif /* NoteGridStyleSheet_hpp */
