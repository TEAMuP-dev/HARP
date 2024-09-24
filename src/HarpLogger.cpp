#include "HarpLogger.h"

// Get the singleton instance of HarpLogger
HarpLogger& HarpLogger::getInstance()
{
    static HarpLogger instance;  // Singleton instance
    return instance;
}

// Log a message
void HarpLogger::LogAndDBG(const juce::String& message) const
{
    DBG(message);
    if (logger)
    {
        logger->logMessage(message);
    }
}

// Initialize the logger
void HarpLogger::initializeLogger()
{
    // logger.reset(new juce::FileLogger(juce::File(logFilePath), logStartMessage));
    logger.reset(juce::FileLogger::createDefaultAppLogger("HARP", "harp.log", "hello, harp!"));
    LogAndDBG("Logger initialized with reset().");
}

juce::File HarpLogger::getLogFile() const
{
    return logger->getLogFile();
}