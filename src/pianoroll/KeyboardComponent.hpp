// Adapted from https://github.com/Sjhunt93/Piano-Roll-Editor

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"

using namespace juce;

class KeyboardComponent : public Component
{
public:
    KeyboardComponent();

    ~KeyboardComponent();

    float getKeyHeight();

    void paint(Graphics& g);

    static const char* pitchNames[];
    static const Array<int> blackPitches;

    virtual bool isKeyboardComponent() { return true; }
};
