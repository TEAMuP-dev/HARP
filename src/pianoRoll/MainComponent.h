/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "PianoRollEditorComponent.hpp"


class MainComponent   : public Component, public Timer
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent();

    //==============================================================================
    void paint (Graphics&) override;
    void resized() override;

    void timerCallback() override;

private:
    //==============================================================================
    // Your private member variables go here...
    st_int tickTest;
    PianoRollEditorComponent editor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
