/**
 * @file
 * @brief A GradioClient class for making http requests to the Gradio API
 * @author  xribene
 */

#pragma once

#include "juce_core/juce_core.h"
#include <fstream>

#include "../HarpLogger.h"
#include "../errors.h"
#include "../utils.h"
#include "Client.h"

using namespace juce;

class GradioClient : public Client
{
public:
    GradioClient();
    ~GradioClient() = default;

    // Space Info
    OpResult setSpaceInfo(const SpaceInfo& info) override;

    // Requests
    OpResult getControls(Array<var>& inputComponents,
                         Array<var>& outputComponents,
                         DynamicObject& cardDict) override;
    OpResult uploadFileRequest(const File& fileToUpload,
                               String& uploadedFilePath,
                               const int timeoutMs = 10000) const override;
    OpResult processRequest(Error&, String&, std::vector<String>&, LabelList&) override;
    OpResult cancel() override;

    // Authorization
    OpResult validateToken(const String& newToken) const override;

private:
    OpResult extractKeyFromResponse(const String& response,
                                    String& responseKey,
                                    const String& key) const;

    OpResult makePostRequestForEventID(const String endpoint,
                                       String& eventId,
                                       const String jsonBody = R"({"data": []})",
                                       const int timeoutMs = 10000) const;

    OpResult getResponseFromEventID(const String callID,
                                    const String eventID,
                                    String& response,
                                    const int timeoutMs = 10000) const;

    OpResult downloadFileFromURL(const URL& fileURL,
                                 String& downloadedFilePath,
                                 const int timeoutMs = 10000) const;
};
