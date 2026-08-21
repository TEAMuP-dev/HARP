/**
 * @file Logging.h
 * @brief Handles logging to terminal and file.
 * @author xribene
 */

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include "Errors.h"

using namespace juce;

class HARPLogger : private DeletedAtShutdown
{
public:
    JUCE_DECLARE_SINGLETON(HARPLogger, false)

    ~HARPLogger()
    {
        logger.reset(); // Explicitly reset pointer to release FileLogger

        clearSingletonInstance();
    }

    // Disable copy constructor
    HARPLogger(const HARPLogger&) = delete;
    // Disable assignment operator
    HARPLogger& operator=(const HARPLogger&) = delete;

    void initializeLogger()
    {
        // MacOS: ~/Library/Logs/HARP/main.log
        // Windows: C:\Users\<username>\AppData\Roaming\HARP\main.log
        // Linux: ~/.config/HARP/main.log
        logger.reset(FileLogger::createDefaultAppLogger("HARP", "main.log", ""));
    }

    void debugAndLog(const String& message) const
    {
        DBG(message); // Write to console

        const ScopedLock lock(loggerLock);

        if (logger)
        {
            logger->logMessage(message); // Write to file
        }
    }

    File getLogFile() const
    {
        const ScopedLock lock(loggerLock);
        return logger != nullptr ? logger->getLogFile() : File();
    }

    OpResult clearLog()
    {
        const ScopedLock lock(loggerLock);

        File logFile = logger->getLogFile();
        logger.reset(); // release file handle before truncating
        bool truncated = logFile.replaceWithText("");
        initializeLogger(); // reopen

        File launchLog =
            FileLogger::getSystemLogFileFolder().getChildFile("HARP").getChildFile("launch.log");
        if (launchLog.existsAsFile())
            truncated = launchLog.replaceWithText("") && truncated;

        if (! truncated)
        {
            return OpResult::fail(
                FileError { FileError::Type::WriteFailed, logFile.getFullPathName() });
        }

        return OpResult::ok();
    }

private:
    HARPLogger() = default; // Prevents instantiation from outside

    mutable CriticalSection loggerLock;

    std::unique_ptr<FileLogger> logger { nullptr };
};

/**
 * Helper function to simplify logging calls.
 */
inline void debugAndLog(const String& message) { HARPLogger::getInstance()->debugAndLog(message); }

/**
 * Macro to match the behavior of DBG().
 */
#define DBG_AND_LOG(textToWrite)                                                   \
    JUCE_BLOCK_WITH_FORCED_SEMICOLON(String tempDbgBuf; tempDbgBuf << textToWrite; \
                                     debugAndLog(tempDbgBuf);)
