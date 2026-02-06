/**
 * @file Settings.h
 * @brief Handles storage and reading of persistent settings.
 * @author xribene, lindseydeng, cwitkowitz
 */

#pragma once

#include <juce_core/juce_core.h>

using namespace juce;

/**
 * Helper class to access settings throughout the application
 * using a singleton pattern with explicit initialization.
 */
class Settings
{
public:
    // Disable copy constructor
    Settings(const Settings&) = delete;
    // Disable assignment operator
    Settings& operator=(const Settings&) = delete;

    /**
     * Initialize settings with provided application properties.
     */
    static void initialize(ApplicationProperties* properties)
    {
        getInstance()->appProperties = properties;
    }

    /**
     * Get user settings for application or return a null pointer if none exist.
     */
    static PropertiesFile* getUserSettings()
    {
        return getInstance()->appProperties != nullptr
                   ? getInstance()->appProperties->getUserSettings()
                   : nullptr;
    }

    /**
     * Check if a key exists in the settings.
     */
    static bool containsKey(const String& keyName)
    {
        if (auto* settings = getUserSettings())
        {
            return settings->containsKey(keyName);
        }

        return false;
    }

    /**
     * Get a string value with an optional default.
     */
    static String getString(const String& keyName, const String& defaultValue = {})
    {
        if (auto* settings = getUserSettings())
        {
            return settings->getValue(keyName, defaultValue);
        }

        return defaultValue;
    }

    /**
     * Get an integer value with an optional default.
     */
    static int getIntValue(const String& keyName, int defaultValue = 0)
    {
        if (auto* settings = getUserSettings())
        {
            return settings->getIntValue(keyName, defaultValue);
        }

        return defaultValue;
    }

    /**
     * Get a double value with an optional default.
     */
    static double getDoubleValue(const String& keyName, double defaultValue = 0.0)
    {
        if (auto* settings = getUserSettings())
        {
            return settings->getDoubleValue(keyName, defaultValue);
        }

        return defaultValue;
    }

    /**
     * Get a boolean value with an optional default.
     */
    static bool getBoolValue(const String& keyName, bool defaultValue = false)
    {
        if (auto* settings = getUserSettings())
        {
            return settings->getBoolValue(keyName, defaultValue);
        }

        return defaultValue;
    }

    /**
     * Set a boolean value as a string.
     */
    static void setValue(const String& keyName, bool value, bool saveImmediately = false)
    {
        if (auto* settings = getUserSettings())
        {
            settings->setValue(keyName, value ? "true" : "false");

            if (saveImmediately)
            {
                settings->saveIfNeeded();
            }
        }
    }

    /**
     * Set a value for any type that can be converted to a string.
     */
    template <typename ValueType>
    static void
        setValue(const String& keyName, const ValueType& value, bool saveImmediately = false)
    {
        if (auto* settings = getUserSettings())
        {
            settings->setValue(keyName, String(value));

            if (saveImmediately)
            {
                settings->saveIfNeeded();
            }
        }
    }

    /**
     * Save settings to disk if there have been changes.
     */
    static void saveIfNeeded()
    {
        if (auto* settings = getUserSettings())
        {
            settings->saveIfNeeded();
        }
    }

    /**
     * Remove an entry from the settings.
     */
    static void removeValue(const String& keyName, bool saveImmediately = false)
    {
        if (auto* settings = getUserSettings())
        {
            if (settings->containsKey(keyName))
            {
                settings->removeValue(keyName);

                if (saveImmediately)
                {
                    settings->saveIfNeeded();
                }
            }
        }
    }

private:
    Settings() : appProperties(nullptr) {}

    static Settings* getInstance()
    {
        static Settings instance;

        return &instance;
    }

    ApplicationProperties* appProperties;
};
