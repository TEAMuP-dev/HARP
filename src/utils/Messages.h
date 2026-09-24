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

struct StatusMessage : SharedMessage
{
    static constexpr int maxHistoryEntries = 250;

    void setMessage(const String& m) override
    {
        {
            const ScopedLock lock(messageLock);
            message = m;

            if (m.isNotEmpty())
            {
                String timestamp = Time::getCurrentTime().formatted("%H:%M:%S");
                history.add("[" + timestamp + "] " + m);

                while (history.size() > maxHistoryEntries)
                {
                    history.remove(0);
                }
            }
        }

        sendChangeMessage();
    }

    void clearMessage() override
    {
        {
            const ScopedLock lock(messageLock);
            message.clear();
            history.clear();
        }

        sendChangeMessage();
    }

    String getHistoryText() const
    {
        const ScopedLock lock(messageLock);
        return history.joinIntoString("\n");
    }

private:
    StringArray history;
};

struct InstructionsMessage : SharedMessage
{
};
