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
    enum Status
    {
        GRADIO,
        HUGGINGFACE,
        LOCALHOST,
        ERROR,
        EMPTY
    };
    juce::String huggingface;
    juce::String gradio;
    juce::String userInput;
    juce::String modelName;
    juce::String userName;
    juce::String error;
    Status status;

    SpaceInfo() : status(Status::EMPTY) {}

    juce::String getStatusString() const
    {
        switch (status)
        {
            case GRADIO: return "Gradio";
            case HUGGINGFACE: return "HuggingFace";
            case LOCALHOST: return "Localhost";
            case ERROR: return "Error";
            case EMPTY: return "Empty";
            default: return "Unknown";
        }
    }
    juce::String toString()
    {
        juce::String str = "SpaceInfo: \n";
        str += "Huggingface: " + huggingface + "\n";
        str += "Gradio: " + gradio + "\n";
        str += "UserInput: " + userInput + "\n";
        str += "ModelName: " + modelName + "\n";
        str += "UserName: " + userName + "\n";
        str += "Status: " + getStatusString() + "\n";
        str += "Error: " + error + "\n";
        return str;
    }
    
};