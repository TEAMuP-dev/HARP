/**
 * @file
 * @brief Utility structs and enums used in the application
 * @author xribene
 */

#pragma once

#include "juce_core/juce_core.h"

enum ModelStatus
{
    INITIALIZED,

    LOADING,
    GETTING_CONTROLS,
    LOADED,
    
    STARTING,
    SENDING,
    PROCESSING,
    FINISHED,
    CANCELLED,
    CANCELLING,
    
    ERROR
};

struct Ctrl
{
    juce::Uuid id { "" };
    std::string label { "" };
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
            case GRADIO:
                return "Gradio";
            case HUGGINGFACE:
                return "HuggingFace";
            case LOCALHOST:
                return "Localhost";
            case ERROR:
                return "Error";
            case EMPTY:
                return "Empty";
            default:
                return "Unknown";
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

class OutputLabel
{
public:
    virtual ~OutputLabel() = default; // virtual destructor
    // TODO: maybe to_y() needs a input parameter to calculate the y value
    virtual std::optional<float> to_y() = 0;

    // required on pyharp side
    float t;
    juce::String label;
    // optional on pyharp side
    juce::String description { "" };
    std::optional<float> duration;
};

class AudioLabel : public OutputLabel
{
public:
    std::optional<float> to_y() override
    {
        // TODO: Implement this
        // You can check if the amplitude has been set
        // by checking if the optional has a value
        // if (amplitude.has_value())
        return amplitude;
    }
    // Optional on pyharp side
    std::optional<float> amplitude;
};

class SpectrogramLabel : public OutputLabel
{
public:
    std::optional<float> to_y() override
    {
        return frequency;
    }
    // Optional on pyharp side
    std::optional<float> frequency;
};

class MidiLabel : public OutputLabel
{
public:
    std::optional<float> to_y() override
    {
        return pitch;
    }
    // Optional on pyharp side
    std::optional<float> pitch;
};

using CtrlList = std::vector<std::pair<juce::Uuid, std::shared_ptr<Ctrl>>>;
using LabelList = std::vector<std::unique_ptr<OutputLabel>>;
