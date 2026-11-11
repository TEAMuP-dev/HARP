/**
 * @file Logging.h
 * @brief Handles logging to terminal and file.
 * @author cwitkowitz, saumya-pailwan, xribene
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

    /* Opens the log, unless it is already open. The application and every window ask for
       it, and worker threads may be writing to it at the time, so it is replaced only under
       the lock.

       macOS: ~/Library/Logs/HARP/main.log
       Windows: C:\Users\<username>\AppData\Roaming\HARP\main.log
       Linux: ~/.config/HARP/main.log */
    void initializeLogger()
    {
        const ScopedLock lock(loggerLock);

        if (logger == nullptr)
        {
            // Heads every session, so that a log attached to a report says what wrote it
            const String welcomeMessage = String(APP_NAME) + " " + String(APP_VERSION) + " on "
                                          + SystemStats::getOperatingSystemName();

            logger.reset(FileLogger::createDefaultAppLogger(APP_NAME, "main.log", welcomeMessage));

            // Launches were logged to a file of their own up to 4.0.0
            logger->getLogFile().getSiblingFile("launch.log").deleteFile();
        }
    }

    void debugAndLog(const String& message) const
    {
        const String timestampedMessage = "[" + getTimestamp() + "] " + message;

        const ScopedLock lock(loggerLock);

        if (logger != nullptr)
        {
            // Written to the file, and to the console as well in a debug build
            logger->logMessage(timestampedMessage);
        }
        else
        {
            DBG(timestampedMessage);
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

        if (logger == nullptr)
        {
            return OpResult::fail(FileError { FileError::Type::WriteFailed, "" });
        }

        File logFile = logger->getLogFile();
        logger.reset(); // release file handle before truncating
        const bool truncated = logFile.replaceWithText("");
        initializeLogger(); // reopen

        if (! truncated)
        {
            return OpResult::fail(
                FileError { FileError::Type::WriteFailed, logFile.getFullPathName() });
        }

        return OpResult::ok();
    }

private:
    HARPLogger() = default; // Prevents instantiation from outside

    // Local time to the millisecond, e.g. "2026-11-10 14:35:12.048"
    static String getTimestamp()
    {
        const Time now = Time::getCurrentTime();

        return now.formatted("%Y-%m-%d %H:%M:%S") + "."
               + String(now.getMilliseconds()).paddedLeft('0', 3);
    }

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
