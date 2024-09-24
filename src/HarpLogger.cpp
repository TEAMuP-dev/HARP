#include "HarpLogger.h"

JUCE_IMPLEMENT_SINGLETON (HarpLogger)

HarpLogger::~HarpLogger()
{
    logger.reset();  // Explicitly reset the unique pointer to release the FileLogger
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
    logger.reset(juce::FileLogger::createDefaultAppLogger("HARP", "harp.log", "hello, harp!"));
}

juce::File HarpLogger::getLogFile() const
{
    return logger->getLogFile();
}