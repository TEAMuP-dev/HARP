/**
 * @file
 * @brief Utility structs and enums used in the application
 * @author xribene
 */

#pragma once

#include "external/magic_enum.hpp"
#include "juce_core/juce_core.h"

// A function to convert a boolean string to a c++ boolean value
// JUCE doesn't have a built-in function to do this
inline bool stringToBool(const juce::String& str) {
    juce::String lowerStr = str.toLowerCase();
    if (lowerStr == "true" || lowerStr == "1" || lowerStr == "yes" || lowerStr == "y")
        return true;
    return false;
}

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

struct PyHarpComponentInfo
{
    juce::Uuid id { "" };
    std::string label { "" };
    virtual ~PyHarpComponentInfo() = default; // virtual destructor
};

struct SliderInfo : public PyHarpComponentInfo
{
    double minimum;
    double maximum;
    double step;
    double value;
};

struct TextBoxInfo : public PyHarpComponentInfo
{
    std::string value;
};

struct AudioTrackInfo : public PyHarpComponentInfo
{
    std::string value;
    bool required;
};

struct MidiTrackInfo : public PyHarpComponentInfo
{
    std::string value;
    bool required;
};

struct NumberBoxInfo : public PyHarpComponentInfo
{
    double min;
    double max;
    double value;
};

struct ToggleInfo : public PyHarpComponentInfo
{
    bool value;
};

struct ComboBoxInfo : public PyHarpComponentInfo
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
        STABILITY,
        FAILED,
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
            case STABILITY:
                return "Stability";
            case FAILED:
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

    juce::String getModelSlashUser() const
    {
        if (status == LOCALHOST)
        {
            return "localhost";
        }
        else
        {
            return userName + "/" + modelName;
        }
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

using ComponentInfoList = std::vector<std::pair<juce::Uuid, std::shared_ptr<PyHarpComponentInfo>>>;
using ComponentInfoMap = std::map<juce::Uuid, std::shared_ptr<PyHarpComponentInfo>>;
using LabelList = std::vector<std::unique_ptr<OutputLabel>>;
