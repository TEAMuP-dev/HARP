#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include "client/Client.h"

using namespace juce;

class HarpLogger : private DeletedAtShutdown
{
public:
    // singleton instance of Logger
    JUCE_DECLARE_SINGLETON(HarpLogger, false)

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

// Function for easier access to logging
// without having to write HarpLogger::getInstance()->LogAndDBG(message) every time
inline void LogAndDBG(const juce::String& message)
{
    HarpLogger::getInstance()->LogAndDBG(message);
}
