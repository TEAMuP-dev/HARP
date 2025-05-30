/**
 * @file
 * @brief A StabilityClient class for making http requests to the Gradio API
 * @author  xribene, huiranyu
 */

#pragma once

#include <fstream>

#include "../HarpLogger.h"
#include "../errors.h"
#include "../utils.h"
#include "client.h"
#include "juce_core/juce_core.h"
class StabilityClient : public Client

{
public:
    // StabilityClienty(const juce::String& spaceUrl);
    StabilityClient() = default;

    // Space Info
    OpResult setSpaceInfo(const SpaceInfo& info) override;
    SpaceInfo getSpaceInfo() const override;

    // Requests
    OpResult uploadFileRequest(const juce::File& fileToUpload,
                               juce::String& uploadedFilePath,
                               const int timeoutMs = 10000) const override;
    OpResult processRequest(Error&,
                            juce::String&,
                            std::vector<juce::String>&,
                            LabelList&) override;
    OpResult getControls(juce::Array<juce::var>& inputComponents,
                         juce::Array<juce::var>& outputComponents,
                         juce::DynamicObject& cardDict) override;

    OpResult cancel() override;

    // Authorization    
    void setToken(const juce::String& token) override;

    juce::String getToken() const override;

    void setTokenEnabled(bool enabled) override;

    OpResult validateToken(const juce::String& token) const override;

private:
    
    juce::String getAuthorizationHeader() const;
    juce::String getJsonContentTypeHeader(juce::String&) const;
    juce::String getAcceptHeader() const;
    juce::String createCommonHeaders() const;
    juce::String createJsonHeaders(juce::String&) const;
    OpResult buildPayload(juce::String&, juce::String&, juce::String&) const;

    SpaceInfo spaceInfo;
    juce::String token = "TBD";
    // A bool flag that could be controlled by a checkbox
    bool tokenEnabled = true;
};