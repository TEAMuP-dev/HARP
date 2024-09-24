#include "HarpLogger.h"

// Get the singleton instance of HarpLogger
HarpLogger& HarpLogger::getInstance()
{
    static HarpLogger instance;  // Singleton instance

    // // Register the cleanup function to destroy the logger earlier
    // static bool isRegistered = false;
    // if (!isRegistered)
    // {
    //     std::atexit([]() {
    //         // Explicitly call the destructor of the singleton instance
    //         instance.~HarpLogger();
    //     });
    //     isRegistered = true;
    // }
    
    return instance;
}

HarpLogger::~HarpLogger()
{
    logger.reset();  // Explicitly reset the unique pointer to release the FileLogger
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
    // logger.reset(new juce::FileLogger(juce::File::getCurrentWorkingDirectory().getChildFile ("test.log"), "KAREKLA"));
    
    logger.reset(juce::FileLogger::createDefaultAppLogger("HARP", "harp.log", "hello, harp!"));
    // LogAndDBG("Logger initialized with reset().");

    // logger = std::make_unique<juce::FileLogger>(juce::File::getCurrentWorkingDirectory().getChildFile ("test.log"), "KAREKLA");
        // juce::FileLogger::createDefaultAppLogger("HARP", "harp.log", "hello, harp!"));
}

juce::File HarpLogger::getLogFile() const
{
    return logger->getLogFile();
}