/**
 * @file Messages.h
 * @brief Shared message resources used across UI and clients.
 * @author saumya-pailwan
 */

#pragma once

#include <juce_core/juce_core.h>

using namespace juce;

struct SharedMessage : public ChangeBroadcaster
{
    virtual ~SharedMessage() = default;

    virtual void setMessage(const String& m)
    {
        {
            const ScopedLock lock(messageLock);
            message = m;
        }

        sendChangeMessage();
    }

    virtual void clearMessage()
    {
        {
            const ScopedLock lock(messageLock);
            message.clear();
        }

        sendChangeMessage();
    }

    String getMessage() const
    {
        const ScopedLock lock(messageLock);
        return message;
    }

protected:
    mutable CriticalSection messageLock;
    String message;
};

struct StatusHistorySnapshot
{
    uint64 revision = 0;
    String lastEntry;
    uint64 trimRevision = 0;
    uint64 clearRevision = 0;
};

struct StatusMessage : SharedMessage
{
    static constexpr int maxHistoryEntries = 250;

    void setMessage(const String& m) override
    {
        {
            const ScopedLock lock(messageLock);
            message = m;
            appendHistoryEntryUnsafe(m);
        }

        sendChangeMessage();
    }

    void clearMessage() override
    {
        {
            const ScopedLock lock(messageLock);
            message.clear();
            history.clear();
            ++revision;
            ++clearRevision;
            lastEntry.clear();
        }

        sendChangeMessage();
    }

    String getHistoryText() const
    {
        const ScopedLock lock(messageLock);
        return history.joinIntoString("\n");
    }

    StatusHistorySnapshot getHistorySnapshot()
    {
        const ScopedLock lock(messageLock);
        return StatusHistorySnapshot { revision, lastEntry, trimRevision, clearRevision };
    }

private:
    void appendHistoryEntryUnsafe(const String& entryText)
    {
        if (entryText.isEmpty())
        {
            return;
        }

        String timestamp = Time::getCurrentTime().formatted("%H:%M:%S");

        lastEntry = "[" + timestamp + "] " + entryText;
        history.add(lastEntry);

        bool didTrim = false;

        while (history.size() > maxHistoryEntries)
        {
            history.remove(0);
            didTrim = true;
        }

        ++revision;

        if (didTrim)
        {
            ++trimRevision;
        }
    }

    uint64 revision = 0;
    uint64 trimRevision = 0;
    uint64 clearRevision = 0;
    String lastEntry;
    StringArray history;
};

struct InstructionsMessage : SharedMessage
{
};
