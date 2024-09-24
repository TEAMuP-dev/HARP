#pragma once

// #include <JuceHeader.h> // Include JUCE headers
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include "gradio/GradioClient.h"

using namespace juce;

class HarpLogger
{
public:
    // singleton instance of Logger
    static HarpLogger& getInstance();

    // destructor
    ~HarpLogger();

    // Disable copy constructor and assignment operator
    HarpLogger(const HarpLogger&) = delete;
    HarpLogger& operator=(const HarpLogger&) = delete;

    void LogAndDBG(const juce::String& message) const;

    void initializeLogger();

    juce::File getLogFile() const;

private:
    // Private constructor to prevent instantiation from outside
    HarpLogger() = default;

    std::unique_ptr<juce::FileLogger> logger { nullptr };
};

// Free-standing function for easier access to logging
inline void LogAndDBG(const juce::String& message) { HarpLogger::getInstance().LogAndDBG(message); }