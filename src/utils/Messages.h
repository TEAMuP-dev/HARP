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
            ++snapshot.revision;
            ++snapshot.clearRevision;
            snapshot.lastEntry.clear();
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
        return snapshot;
    }

    // Returns history entries added after sinceRevision, in order
    // Thread-safe, handles coalesced setMessage() calls correctly
    StringArray getEntriesSince(uint64 sinceRevision) const
    {
        const ScopedLock lock(messageLock);

        if (history.isEmpty())
            return {};
        // history[0] was the (snapshot.revision - history.size() + 1)-th entry added.
        // Compute the 0-based index of the first entry we need (revision > sinceRevision).
        // startIdx = sinceRevision - (snapshot.revision - history.size())
        //          = sinceRevision + history.size() - snapshot.revision
        int64 startIdx =
            (int64) sinceRevision + (int64) history.size() - (int64) snapshot.revision;

        if (startIdx < 0)
            startIdx = 0; // All stored entries are newer than sinceRevision

        if (startIdx >= (int64) history.size())
            return {};

        StringArray result;
        for (int i = (int) startIdx; i < history.size(); ++i)
            result.add(history[i]);
        return result;
    }

private:
    void appendHistoryEntryUnsafe(const String& entryText)
    {
        if (entryText.isEmpty())
        {
            return;
        }

        String timestamp = Time::getCurrentTime().formatted("%H:%M:%S");

        snapshot.lastEntry = "[" + timestamp + "] " + entryText;
        history.add(snapshot.lastEntry);

        bool didTrim = false;

        while (history.size() > maxHistoryEntries)
        {
            history.remove(0);
            didTrim = true;
        }

        ++snapshot.revision;

        if (didTrim)
        {
            ++snapshot.trimRevision;
        }
    }

    StatusHistorySnapshot snapshot;
    StringArray history;
};

struct InstructionsMessage : SharedMessage
{
};
