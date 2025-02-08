/**
 * @file
 * @brief Utility structs and enums used in the application
 * @author xribene
 */

#pragma once

#include "external/magic_enum.hpp"
#include "juce_core/juce_core.h"

template <typename EnumType>
inline juce::String enumToString(EnumType enumValue)
{
    return juce::String(magic_enum::enum_name(enumValue).data());
}

enum GradioEvents
{
    complete,
    error,
    heartbeat,
    generating
};

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

struct OutputLabel
{
    // required on pyharp side
    float t;
    juce::String label;
    // optional on pyharp side
    std::optional<juce::String> description;
    std::optional<float> duration;
    std::optional<int> color;
    std::optional<juce::String> link;
    virtual ~OutputLabel() = default; // virtual destructor
};

struct AudioLabel : public OutputLabel
{
    // Optional on pyharp side
    std::optional<float> amplitude;
};

struct SpectrogramLabel : public OutputLabel
{
    // Optional on pyharp side
    std::optional<float> frequency;
};

struct MidiLabel : public OutputLabel
{
    // Optional on pyharp side
    std::optional<float> pitch;
};

using CtrlList = std::vector<std::pair<juce::Uuid, std::shared_ptr<Ctrl>>>;
using LabelList = std::vector<std::unique_ptr<OutputLabel>>;
