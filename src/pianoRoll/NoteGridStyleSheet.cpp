//
//  NoteGridStyleSheet.cpp
//  PianoRollEditor - App
//
//  Created by Samuel Hunt on 03/02/2020.
//

#include "NoteGridStyleSheet.hpp"

NoteGridStyleSheet::NoteGridStyleSheet ()
{
    drawMIDINum = false;
    drawMIDINoteStr = false;
    drawVelocity = true;
    disableEditing = false;
}

bool NoteGridStyleSheet::getDrawMIDINum ()
{
    return drawMIDINum;
}
bool NoteGridStyleSheet::getDrawMIDINoteStr ()
{
    return drawMIDINoteStr;
}
bool NoteGridStyleSheet::getDrawVelocity ()
{
    return drawVelocity;
}
