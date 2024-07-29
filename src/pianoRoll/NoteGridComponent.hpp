#pragma once

#ifndef NoteGridComponent_hpp
#define NoteGridComponent_hpp

#include "PNoteComponent.hpp"

using namespace juce;


class NoteGridComponent : public Component
{
public:

    NoteGridComponent();

    ~NoteGridComponent();

    void paint(Graphics& g) override;
    void resized() override;

    void setPositions();

    void setupGrid(float pixelsPerBar, float compHeight, const int bars);

    void loadSequence(PRESequence sq);

    float getNoteCompHeight();
    float getPixelsPerBar();

private:

    std::vector<PNoteComponent*> noteComps;

    Array<int> blackPitches;

    float noteCompHeight;
    float pixelsPerBar;

    st_int ticksPerTimeSignature;

};

#endif /* NoteGridComponent_hpp */
