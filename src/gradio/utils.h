#pragma once

#include "juce_core/juce_core.h"

struct Ctrl 
{
    juce::Uuid id {""};
    std::string label {""};
    virtual ~Ctrl() = default; // virtual destructor
};

struct SliderCtrl : public Ctrl
{
    double minimum;
    double maximum;
    double step;
    double value;
};

struct TextBoxCtrl : public Ctrl 
{
    std::string value;
};

struct AudioInCtrl : public Ctrl 
{
    std::string value;
};

struct MidiInCtrl : public Ctrl 
{
    std::string value;
};

struct NumberBoxCtrl : public Ctrl 
{
    double min;
    double max;
    double value;
};

struct ToggleCtrl : public Ctrl
{
    bool value;
};

struct ComboBoxCtrl : public Ctrl
{
    std::vector<std::string> options;
    std::string value;
};

struct SpaceInfo
{
    juce::String huggingface;
    juce::String gradio;
    juce::String userInput;
    juce::String modelName;
    juce::String userName;
    juce::String error;
};