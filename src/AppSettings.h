/**
 * ChatGPT generated code
 */
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Helper class to easily access application settings throughout the app
 * using a singleton pattern with explicit initialization.
 */
class AppSettings
{
public:
    /** Initialize the settings with the application properties */
    static void initialize(juce::ApplicationProperties* properties)
    {
        getInstance()->appProperties = properties;
    }
    
    /** Get the application's user settings */
    static juce::PropertiesFile* getUserSettings()
    {
        return getInstance()->appProperties != nullptr ? 
               getInstance()->appProperties->getUserSettings() : nullptr;
    }
    
    /** Get a string value with optional default */
    static juce::String getString(const juce::String& keyName, const juce::String& defaultValue = {})
    {
        if (auto* settings = getUserSettings())
            return settings->getValue(keyName, defaultValue);
        
        return defaultValue;
    }
    
    /** Get an integer value with optional default */
    static int getIntValue(const juce::String& keyName, int defaultValue = 0)
    {
        if (auto* settings = getUserSettings())
            return settings->getIntValue(keyName, defaultValue);
        
        return defaultValue;
    }
    
    /** Get a double value with optional default */
    static double getDoubleValue(const juce::String& keyName, double defaultValue = 0.0)
    {
        if (auto* settings = getUserSettings())
            return settings->getDoubleValue(keyName, defaultValue);
        
        return defaultValue;
    }
    
    /** Get a boolean value with optional default */
    static bool getBoolValue(const juce::String& keyName, bool defaultValue = false)
    {
        if (auto* settings = getUserSettings())
            return settings->getBoolValue(keyName, defaultValue);
        
        return defaultValue;
    }

    // Set value specific for boolean type
    static void setValue(const juce::String& keyName, bool value, bool saveImmediately = false)
    {
        if (auto* settings = getUserSettings())
        {
            settings->setValue(keyName, value ? "true" : "false");

             if (saveImmediately)
            settings->saveIfNeeded();
    }
}
  
 
   /** Set a value of any type that can be converted to string */
    template <typename ValueType>
    static void setValue(const juce::String& keyName, const ValueType& value, bool saveImmediately = false)
    {   
        if (auto* settings = getUserSettings())
        {
            settings->setValue(keyName, juce::String(value));

            if (saveImmediately)
                settings->saveIfNeeded();
        }
}
    
    /** Save settings to disk if needed */
    static void saveIfNeeded()
    {
        if (auto* settings = getUserSettings())
            settings->saveIfNeeded();
    }
    
    /** Check if a key exists in the settings */
    static bool containsKey(const juce::String& keyName)
    {
        if (auto* settings = getUserSettings())
            return settings->containsKey(keyName);
            
        return false;
    }
    
    /** Remove a key from the settings */
    static void removeValue(const juce::String& keyName, bool saveImmediately = false)
    {
        if (auto* settings = getUserSettings())
        {
            // Check if the key exists before removing
            if (settings->containsKey(keyName))
            {
                settings->removeValue(keyName);
            
                if (saveImmediately)
                    settings->saveIfNeeded();
            }
            
        }
    }

private:
    // Singleton instance getter
    static AppSettings* getInstance()
    {
        static AppSettings instance;
        return &instance;
    }
    
    // Private constructor for singleton
    AppSettings() : appProperties(nullptr) {}
    
    // Delete copy constructor and assignment operator
    AppSettings(const AppSettings&) = delete;
    AppSettings& operator=(const AppSettings&) = delete;
    
    // The application properties instance
    juce::ApplicationProperties* appProperties;
};
