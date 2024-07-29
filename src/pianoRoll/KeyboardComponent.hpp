#pragma once

#ifndef KeyboardComponent_hpp
#define KeyboardComponent_hpp

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;


class KeyboardComponent : public Component
{
public:
    
    KeyboardComponent();

    void paint(Graphics& g);

private:

    Array<int> blackPitches;
};

#endif /* KeyboardComponent_hpp */
