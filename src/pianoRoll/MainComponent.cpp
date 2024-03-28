/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    setSize (800, 600);
    addAndMakeVisible(editor);
    editor.setup(10, 900, 20); //default 10 bars, with 900 pixels per bar (width) and 20 pixels per step (each note height)
    
    editor.sendChange = [](int note, int velocity)
    {
        // You will probably want to send this information to some kind of MIDI messaging system
        std::cout << "MIDI send: " << note << " : " << velocity << "\n";
    };
    tickTest = 0;
    startTimer(20);
}

MainComponent::~MainComponent()
{
    
}

//==============================================================================
void MainComponent::paint (Graphics& g)
{
    //unused
}

void MainComponent::resized()
{
    editor.setBounds(0, 0, getWidth(), getHeight());
}

void MainComponent::timerCallback()
{
    tickTest += 20;
    editor.setPlaybackMarkerPosition(tickTest);
    if (tickTest >= 480 * 4 * 10) {
        tickTest = 0;
    }
}
