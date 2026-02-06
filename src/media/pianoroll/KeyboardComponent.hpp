/**
 * @file KeyboardComponent.hpp
 * @brief Reusable component with rows of piano keys.
 * @author xribene, cwitkowitz
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

class KeyboardComponent : public Component
{
public:
    KeyboardComponent() {};

    ~KeyboardComponent() {};

    static const char* pitchNames[];
    static const Array<int> blackPitches;

    void paint(Graphics& g);

    virtual bool isKeyboardComponent() { return true; }

    float getKeyHeight();
};
