/**
 * @file
 * @brief Utility structs and enums used in the application
 * @author xribene
 */

#pragma once

#include "external/magic_enum.hpp"
#include "juce_core/juce_core.h"
#include "juce_gui_basics/juce_gui_basics.h"

using namespace juce;

// A function to convert a boolean string to a c++ boolean value
// JUCE doesn't have a built-in function to do this
inline bool stringToBool(const String& str)
{
    String lowerStr = str.toLowerCase();
    if (lowerStr == "true" || lowerStr == "1" || lowerStr == "yes" || lowerStr == "y")
        return true;
    return false;
}

// TODO - move to gui/GUIUtils.h
inline Colour getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour uiColour,
                                     Colour fallback = Colour(0xff4d4d4d)) noexcept
{
    if (auto* v4 = dynamic_cast<LookAndFeel_V4*>(&LookAndFeel::getDefaultLookAndFeel()))
        return v4->getCurrentColourScheme().getUIColour(uiColour);

    return fallback;
}

template <typename EnumType>
inline String enumToString(EnumType enumValue)
{
    return String(magic_enum::enum_name(enumValue).data());
}

enum DisplayMode
{
    Input,
    Output,
    Hybrid, // All functionality
    Thumbnail // Reduced functionality
};

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
    Uuid id { "" };
    std::string label { "" };
    std::string info { "" };
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
    String huggingface;
    String gradio;
    String stability;
    String userInput;
    String modelName;
    String userName;
    String error;
    Status status;
    std::optional<juce::String> stabilityServiceType;
    juce::String apiEndpointURL; // The primary API endpoint URL for the space

    SpaceInfo() : status(Status::EMPTY) {}

    String getStatusString() const
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
                if (stabilityServiceType.has_value())
                    return "Stability (" + stabilityServiceType.value() + ")";
                return "Stability";
            case FAILED:
                return "Error";
            case EMPTY:
                return "Empty";
            default:
                return "Unknown";
        }
    }
    String toString()
    {
        String str = "SpaceInfo: \n";
        str += "UserInput: " + userInput + "\n";
        str += "Status: " + getStatusString() + "\n";
        str += "API Endpoint: " + apiEndpointURL + "\n";
        if (status == STABILITY && stabilityServiceType.has_value())
        {
            str += "Service: " + stabilityServiceType.value() + "\n";
        }
        else
        {
            str += "Huggingface: " + huggingface + "\n";
            str += "Gradio: " + gradio + "\n";
            str += "ModelName: " + modelName + "\n";
            str += "UserName: " + userName + "\n";
        }
        str += "Error: " + error + "\n";
        return str;
    }

    String getModelSlashUser() const
    {
        if (status == LOCALHOST)
        {
            return "localhost";
        }
        else if (status == STABILITY)
        {
            if (stabilityServiceType.has_value())
                return "stability/" + stabilityServiceType.value();
            return "stability/unknown_service";
        }
        else if (userName.isNotEmpty() && modelName.isNotEmpty())
        {
            return userName + "/" + modelName;
        }
        return "Unknown/Unknown";
    }
};

struct OutputLabel
{
    // required on pyharp side
    float t;
    String label;
    // optional on pyharp side
    std::optional<String> description;
    std::optional<float> duration;
    std::optional<int> color;
    std::optional<String> link;
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

using ComponentInfo = std::pair<Uuid, std::shared_ptr<PyHarpComponentInfo>>;
using ComponentInfoMap = std::map<Uuid, std::shared_ptr<PyHarpComponentInfo>>;
using ComponentInfoList = std::vector<ComponentInfo>;

using LabelList = std::vector<std::unique_ptr<OutputLabel>>;
