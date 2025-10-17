/**
 * @file
 * @brief A StabilityClient class for making http requests to the Stability AI's API
 * @author  xribene, huiranyu
 */

#pragma once

#include "juce_core/juce_core.h"
#include <fstream>

#include "../HarpLogger.h"
#include "../errors.h"
#include "../utils.h"
#include "Client.h"

using namespace juce;

class StabilityClient : public Client
{
public:
    StabilityClient();
    ~StabilityClient() = default;

    // Space Info
    OpResult setSpaceInfo(const SpaceInfo& info) override;

    // Requests
    OpResult getControls(Array<var>& inputComponents,
                         Array<var>& outputComponents,
                         DynamicObject& cardDict) override; // TODO - abstract to ThirdPartyClient
    OpResult uploadFileRequest(const File& fileToUpload,
                               String& uploadedFilePath,
                               const int timeoutMs = 10000) const override;
    OpResult processRequest(Error&, String&, std::vector<String>&, LabelList&) override;
    OpResult cancel() override;

private:
    String getAcceptHeader() const;
    String getJsonContentTypeHeader(const String&) const;

    String createJsonHeaders(const String&) const;

    static String getControlValue(const String& label, const Array<var>* dataArray);
    static String mimeForAudioFile(const File& f);

    OpResult buildPayload(StringPairArray& args, String& processID, String& payload) const;

    OpResult processTextToAudio(const Array<var>* dataArray,
                                Error& error,
                                std::vector<String>& outputFilePaths);

    OpResult processAudioToAudio(const Array<var>* dataArray,
                                 Error& error,
                                 std::vector<String>& outputFilePaths);

    std::atomic<bool> shouldCancel { false };
};
