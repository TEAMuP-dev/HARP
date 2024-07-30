//
//  Constants.h
//  PianoRollEditor
//
//  Created by Samuel Hunt on 16/08/2019.
//

#ifndef PConstants_h
#define PConstants_h


/* --------------------------------------------------------------------------------------------------------------------
 This flag is set to disable some of the custom features the piano roll editor is used for in the IGME application for which this code is taken from.
 
 Ensure that the flag LIB_VERSION is enabled this removes some of the IGME specific code, of course you can also delete that manually if needed
-------------------------------------------------------------------------------------------------------------------- */

// #include <JuceHeader.h>
#include "juce_core/juce_core.h"
using namespace juce;
#define LIB_VERSION

#ifndef u8
typedef unsigned char u8;
#endif
#ifndef st_int
typedef unsigned int st_int;
#endif

namespace PRE { // Piano Roll Editor namespace
/*
 Although this could easily be updated 480 works nicely for MIDI timing and is common in MIDI file formats.
 480/16 = 30 hemidemisemiquaver 1/64
 */
static const int defaultResolution = 480; // per quarter note

static const char * pitches_names[] = {
    "C",
    "C#",
    "D",
    "D#",
    "E",
    "F",
    "F#",
    "G",
    "G#",
    "A",
    "A#",
    "B",
};

enum eQuantisationValue {
    eQuantisationValueNone = 0,
    eQuantisationValue1_32,
    eQuantisationValue1_16,
    eQuantisationValue1_8,
    eQuantisationValue1_4,
    eQuantisationValueTotal,
};

const int quantisedDivisionValues[eQuantisationValueTotal] = {
    1,
    (defaultResolution / 8),
    (defaultResolution / 4),
    (defaultResolution / 2),
    defaultResolution
};

}
#endif /* Constants_h */
