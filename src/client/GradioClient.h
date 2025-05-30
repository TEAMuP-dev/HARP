/**
 * @file
 * @brief A GradioClient class for making http requests to the Gradio API
 * @author  xribene
 */

#pragma once

#include <fstream>

#include "../HarpLogger.h"
#include "../errors.h"
#include "../utils.h"
#include "client.h"
#include "juce_core/juce_core.h"
class GradioClient : public Client
{
public:
    // GradioClient(const juce::String& spaceUrl);
    GradioClient() = default;

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

    OpResult extractKeyFromResponse(const juce::String& response,
                                    juce::String& responseKey,
                                    const juce::String& key) const;

    OpResult makePostRequestForEventID(const juce::String endpoint,
                                       juce::String& eventId,
                                       const juce::String jsonBody = R"({"data": []})",
                                       const int timeoutMs = 10000) const;

    OpResult getResponseFromEventID(const juce::String callID,
                                    const juce::String eventID,
                                    juce::String& response,
                                    const int timeoutMs = 10000) const;


    OpResult downloadFileFromURL(const juce::URL& fileURL,
                                 juce::String& downloadedFilePath,
                                 const int timeoutMs = 10000) const;
    
    juce::String getAuthorizationHeader() const;
    juce::String getJsonContentTypeHeader() const;
    juce::String getAcceptHeader() const;
    juce::String createCommonHeaders() const;
    juce::String createJsonHeaders() const;

    SpaceInfo spaceInfo;
    juce::String token;
    // A bool flag that could be controlled by a checkbox
    bool tokenEnabled = true;
};