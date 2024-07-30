//
//  NoteModel.cpp
//  PianoRollEditor - App
//
//  Created by Samuel Hunt on 16/08/2019.
//

#include "NoteModel.hpp"
#ifndef LIB_VERSION
#include "StaticCounter.h"
#include "DataLoggerRoot.h"
#endif

NoteModel::NoteModel ()
{
    note = 0;
    velocity = 0;
    startTime = 0;
    noteLegnth = 0;
#ifndef LIB_VERSION
    uniqueId = StaticCounter::count();
#endif
}
NoteModel::NoteModel (u8 n, u8 v, st_int st, st_int nl, Flags f)
{
    note = n;
    velocity = v;
    startTime = st;
    noteLegnth = nl;
    flags = f;
#ifndef LIB_VERSION
    uniqueId = StaticCounter::count();
#endif
}
NoteModel::NoteModel (const NoteModel & other)
{
    note = other.note;
    velocity = other.velocity;
    startTime = other.startTime;
    noteLegnth = other.noteLegnth;
    flags = other.flags;
#ifndef LIB_VERSION
    uniqueId = other.uniqueId;
#endif
    sendChange = other.sendChange;
}



/*
 Quantising to say 1/8 notes would require an input value of 240 (or 0.5 * the default resolution)
 */
void NoteModel::quantiseModel (int qValue, bool qStartTime, bool qNoteLegnth)
{
    auto quantiseValue = [&](int qDiv, int valueToQuantise) -> int
    {
        const int simpleQLow = (valueToQuantise / qDiv) * qDiv; //use lossey int division
        const int simpleQHigh = ((valueToQuantise / qDiv)+1) * qDiv;
        
        if (valueToQuantise-simpleQLow < simpleQHigh-valueToQuantise) { //lower boundry
            return simpleQLow;
        }
        return simpleQHigh;
    };
    
    if (qStartTime) {
        startTime = quantiseValue(qValue, startTime);
    }
    if (qNoteLegnth) {
        noteLegnth = quantiseValue(qValue, noteLegnth);
        if (noteLegnth == 0) {
            noteLegnth = qValue; //
        }
    }
    
}

bool NoteModel::compare (const NoteModel & other, bool compareUIDs)
{
#define RETURN_IF(A) if(A){return false;}
    
    RETURN_IF(note                  != other.note)
    RETURN_IF(velocity              != other.velocity)
    RETURN_IF(startTime             != other.startTime)
    RETURN_IF(noteLegnth            != other.noteLegnth)
    RETURN_IF(flags.state           != other.flags.state)
    RETURN_IF(flags.isGenerative    != other.flags.isGenerative)
    
#ifndef LIB_VERSION
    if (compareUIDs) {
        RETURN_IF(uniqueId != other.uniqueId)
    }
#endif
    return true;
}


void NoteModel::setNote (u8 _note)
{
    note = _note;
    trigger();
#ifndef LIB_VERSION
    LOG_NOTE_EDITED_PR(note, velocity, startTime, noteLegnth);
#endif
}
void NoteModel::setVelocity (u8 _velocity)
{
    velocity = _velocity;
    trigger();
#ifndef LIB_VERSION
    LOG_NOTE_EDITED_PR(note, velocity, startTime, noteLegnth);
#endif
}
void NoteModel::setStartTime (st_int _time)
{
    startTime = _time;
}
void NoteModel::setNoteLegnth (st_int _len)
{
    noteLegnth = _len;
}

void NoteModel::trigger()
{
    if (sendChange != nullptr) {
        sendChange(note, velocity);
    }
}
void NoteModel::trigger(const u8 n, const u8 vel)
{
    if (sendChange != nullptr) {
        sendChange(n, vel);
    }
}
void PRESequence::print ()
{
    for (auto nm : events) {
        std::cout << (int) nm.getNote() << " - " << (int) nm.getVelocity() << " : " << nm.getStartTime() << " - " << nm.getNoteLegnth() << "\n";
    }
}

