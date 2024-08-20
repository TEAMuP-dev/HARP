/**
 * @file
 * @brief A GradioClient class for making http requests to the Gradio API
 * @author  xribene
 */

#pragma once

#include <fstream>
#include "juce_core/juce_core.h"
#include "utils.h"

using CtrlList = std::vector<std::pair<juce::Uuid, std::shared_ptr<Ctrl>>>;

class GradioClient
{
public:
    // GradioClient(const juce::String& spaceUrl);
    GradioClient() = default;

    // Example method to get controls from the Gradio API
    void getControls(juce::Array<juce::var>& ctrlList, juce::DynamicObject& cardDict, juce::String& error);
    void setBaseUrl(const juce::String& url);
    static juce::String resolveSpaceUrl(juce::String urlOrName);
private:
    
    juce::String baseUrl;
    juce::URL endpoint;
};