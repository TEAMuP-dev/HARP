#include "HarpLogger.h"

JUCE_IMPLEMENT_SINGLETON(HarpLogger)

HarpLogger::~HarpLogger()
{
    logger.reset(); // Explicitly reset the unique pointer to release the FileLogger
    clearSingletonInstance();
}

// Log a message
void HarpLogger::LogAndDBG(const juce::String& message) const
{
    if (logger)
    {
        DBG(message);
        logger->logMessage(message);
    }
}

// Initialize the logger
void HarpLogger::initializeLogger()
{
    // MacOS: ~/Library/Logs/HARP/main.log
    // Linux: ~/.config/HARP/main.log
    // Windows: C:\Users\<username>\AppData\Roaming\HARP\main.log
    logger.reset(juce::FileLogger::createDefaultAppLogger("HARP", "main.log", "Hello, HARP!"));
}

juce::File HarpLogger::getLogFile() const { return logger->getLogFile(); }
