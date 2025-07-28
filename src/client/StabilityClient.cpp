#include "StabilityClient.h"
#include "../errors.h"
#include "../external/magic_enum.hpp"


OpResult StabilityClient::setSpaceInfo(const SpaceInfo& inSpaceInfo)
{
    spaceInfo = inSpaceInfo;
    return OpResult::ok();
}

SpaceInfo StabilityClient::getSpaceInfo() const { return spaceInfo; }

OpResult StabilityClient::uploadFileRequest(const juce::File& fileToUpload,
                                         juce::String& uploadedFilePath,
                                         const int timeoutMs) const
{
    // TBD. We need the original path of the file.
    return OpResult::ok();
}


OpResult StabilityClient::processTextToAudio(const juce::String& prompt,
                                             Error& error,
                                            std::vector<juce::String>& outputFilePaths)
{
    OpResult result = OpResult::ok();
    juce::String processID = juce::Uuid().toString();

    juce::String payload;
    result = buildPayload(prompt, processID, payload);
    if (result.failed())
    return result;

    juce::URL textToAudioUrl("https://api.stability.ai/v2beta/audio/stable-audio-2/text-to-audio");
    juce::URL postReq = textToAudioUrl.withPOSTData(payload);

    juce::StringPairArray responseHeaders;
    int statusCode = 0;

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                    .withExtraHeaders(createJsonHeaders(processID))
                    .withResponseHeaders(&responseHeaders)
                    .withStatusCode(&statusCode)
                    .withNumRedirectsToFollow(5)
                    .withHttpRequestCmd("POST");

    std::unique_ptr<juce::InputStream> stream(postReq.createInputStream(options));

    if (stream == nullptr)
    {
        error.code = statusCode;
        error.devMessage = "Failed to create input stream for POST request to stability/text-to-audio";
        return OpResult::fail(error);
    }

    if (statusCode != 200)
    {
        juce::String responseJson = stream->readEntireStreamAsString();
        error.code = statusCode;
        error.devMessage = "Request failed with status code: " + juce::String(statusCode) + ", " + responseJson;
        return OpResult::fail(error);
    }

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::String fileName = juce::Uuid().toString() + ".wav";
    juce::File downloadedFile = tempDir.getChildFile(fileName);

    std::unique_ptr<juce::FileOutputStream> fileOutput(downloadedFile.createOutputStream());

    if (fileOutput == nullptr || !fileOutput->openedOk())
    {
        error.devMessage = "Failed to create output stream for file: " + downloadedFile.getFullPathName();
        return OpResult::fail(error);
    }

    fileOutput->writeFromInputStream(*stream, stream->getTotalLength());
    outputFilePaths.push_back(juce::URL(downloadedFile).toString(true));

    return result;
}


OpResult StabilityClient::processAudioToAudio(const juce::String& inputFileURL,
                                              Error& error,
                                            std::vector<juce::String>& outputFilePaths)
{
    OpResult result = OpResult::ok();

    // Generate unique boundary for multipart form
    juce::String boundary = "--------" + juce::Uuid().toString() + "--------";

    // Build request body
    juce::MemoryOutputStream requestBody;
    requestBody << "--" << boundary << "\r\n";
    requestBody << "Content-Disposition: form-data; name=\"audio_url\"\r\n\r\n";
    requestBody << inputFileURL << "\r\n";

    requestBody << "--" << boundary << "\r\n";
    requestBody << "Content-Disposition: form-data; name=\"output_format\"\r\n\r\n";
    requestBody << "wav\r\n";

    requestBody << "--" << boundary << "\r\n";
    requestBody << "Content-Disposition: form-data; name=\"strength\"\r\n\r\n";
    requestBody << "0.65\r\n";

    requestBody << "--" << boundary << "--\r\n";

    // Convert body to string
    juce::MemoryBlock requestBlock = requestBody.getMemoryBlock();
    juce::String postDataString(static_cast<const char*>(requestBlock.getData()),
    static_cast<int>(requestBlock.getSize()));

    juce::URL audioToAudioURL("https://api.stability.ai/v2beta/audio/stable-audio-2/audio-to-audio");
    juce::URL postURL = audioToAudioURL.withPOSTData(postDataString);

    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                  .withHttpRequestCmd("POST")
                  .withExtraHeaders("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n" +
                  getAuthorizationHeader() + getAcceptHeader())
                  .withStatusCode(&statusCode)
                  .withConnectionTimeoutMs(10000);

    std::unique_ptr<juce::InputStream> stream = postURL.createInputStream(options);
    if (stream == nullptr)
    {
        error.code = statusCode;
        error.devMessage = "Failed to create input stream for POST request to stability/audio-to-audio.";
        return OpResult::fail(error);
    }

    if (statusCode != 200)
    {
        juce::String response = stream->readEntireStreamAsString();
        error.code = statusCode;
        error.devMessage = "Audio-to-audio request failed: " + response;
        return OpResult::fail(error);
    }

    // Save the response as a .wav file
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::String fileName = juce::Uuid().toString() + ".wav";
    juce::File downloadedFile = tempDir.getChildFile(fileName);

    std::unique_ptr<juce::FileOutputStream> fileOutput(downloadedFile.createOutputStream());
    if (fileOutput == nullptr || !fileOutput->openedOk())
    {
        error.devMessage = "Failed to write audio-to-audio result to: " + downloadedFile.getFullPathName();
        return OpResult::fail(error);
    }

    fileOutput->writeFromInputStream(*stream, stream->getTotalLength());
    outputFilePaths.push_back(URL(downloadedFile).toString(true));

    return OpResult::ok();
}

OpResult StabilityClient::processRequest(Error& error,
                                        juce::String& processingPayload,
                                        std::vector<juce::String>& outputFilePaths,
                                        LabelList& labels)
{
    OpResult result = OpResult::ok();

    // Parse the processingPayload JSON
    juce::var parsedPayload;
    juce::JSON::parse(processingPayload, parsedPayload);

    if (!parsedPayload.isObject())
    {
        error.devMessage = "Failed to parse processingPayload. Not a JSON object.";
        return OpResult::fail(error);
    }

    juce::DynamicObject* obj = parsedPayload.getDynamicObject();
    if (obj == nullptr)
    {
        error.devMessage = "Failed to get dynamic object from processingPayload.";
        return OpResult::fail(error);
    }

    juce::Array<juce::var>* dataArray = obj->getProperty("data").getArray();
    if (dataArray == nullptr || dataArray->isEmpty())
    {
        error.devMessage = "Missing or empty 'data' array in processingPayload.";
        return OpResult::fail(error);
    }

    // Dispatch to correct model endpoint
    if (spaceInfo.modelName == "stability/audio-to-audio")
    {
        juce::String inputAudioUrl = dataArray->getReference(0).toString();
        return processAudioToAudio(inputAudioUrl, error, outputFilePaths);
    }
    else if (spaceInfo.modelName == "stability/text-to-audio")
    {
        juce::String prompt = dataArray->getReference(0).toString();
        return processTextToAudio(prompt, error, outputFilePaths);
    }
    else
    {
        error.devMessage = "Unsupported Stability AI model: " + spaceInfo.modelName;
        return OpResult::fail(error);
    }
}


OpResult StabilityClient::getControls(juce::Array<juce::var>& inputComponents,
                                      juce::Array<juce::var>& outputComponents,
                                      juce::DynamicObject& cardDict)
{
    OpResult result = OpResult::ok();
    juce::String modelName = spaceInfo.modelName.trim();
    DBG("DEBUG: StabilityClient::getControls() - modelName = " + modelName);

    // JSON template depending on the model
    juce::String responseData;


    if (modelName == "stability/audio-to-audio" || modelName == "audio-to-audio")
    {
        responseData = R"(
            [{
            "card": {
            "name": "Audio to Audio",
            "description": "Stability AI audio-to-audio with prompt",
            "author": "Stability",
            "tags": ["audio", "transformation"]
            },
            "inputs": [
                {
                    "label": "Input Audio",
                    "required": true,
                    "type": "audio_track"
                },
                {
                    "label": "Text Prompt",
                    "value": "happy",
                    "type": "text_box"
                }
            ],
            "outputs": [{
                "label": "Output Audio",
                "required": true,
                "type": "audio_track"
            }]
            }]
        )";
    }
    else if (modelName == "stability/text-to-audio" || modelName == "text-to-audio")
    {

        responseData = R"(
            [{
            "card": {
            "name": "Text to Audio",
            "description": "Integrated stability text-to-audio",
            "author": "Stability",
            "tags": ["text", "audio", "generation"]
            },
            "inputs": [{
            "label": "Input Text Prompt",
            "value": "happy",
            "type": "text_box"
            }],
            "outputs": [{
            "label": "Output Audio",
            "required": true,
            "type": "audio_track"
            }]
            }]
            )";
    }
    else
    {
        Error error;
        error.devMessage = "Unsupported model in getControls(): " + spaceInfo.modelName;
        return OpResult::fail(error);
    }

    // Parse the generated JSON
    juce::var parsedData;
    juce::JSON::parse(responseData, parsedData);

    if (!parsedData.isArray())
    {
        Error error;
        error.devMessage = "Failed to parse controls JSON: Not an array.";
        return OpResult::fail(error);
    }

    juce::Array<juce::var>* dataArray = parsedData.getArray();
    if (dataArray == nullptr || dataArray->isEmpty())
    {
        Error error;
        error.devMessage = "Controls JSON array is empty or null.";
        return OpResult::fail(error);
    }

    juce::DynamicObject* obj = dataArray->getFirst().getDynamicObject();
    if (obj == nullptr)
    {
        Error error;
        error.devMessage = "First item in controls JSON is not a valid object.";
        return OpResult::fail(error);
    }

    // Copy card dictionary
    if (auto* cardObj = obj->getProperty("card").getDynamicObject())
    {
        cardDict.clear();
        for (const auto& key : cardObj->getProperties())
        cardDict.setProperty(key.name, key.value);
    }

    // Copy input and output arrays
    if (auto* inputs = obj->getProperty("inputs").getArray())
    inputComponents = *inputs;

    if (auto* outputs = obj->getProperty("outputs").getArray())
    outputComponents = *outputs;

    return result;
}


OpResult StabilityClient::cancel() {

}

juce::String StabilityClient::getAuthorizationHeader() const
{
    if (tokenEnabled && ! token.isEmpty())
    {
        return "Authorization: Bearer " + token + "\r\n";
    }
    return "";
}

juce::String StabilityClient::getJsonContentTypeHeader(const juce::String& processID) const
{
    juce::String boundary = "--------" + processID + "--------";
    return "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
}

juce::String StabilityClient::getAcceptHeader() const { return "Accept: audio/*\r\n"; }

juce::String StabilityClient::createCommonHeaders() const
{
    return getAcceptHeader() + getAuthorizationHeader();
}

juce::String StabilityClient::createJsonHeaders(const juce::String& processID) const
{
    return getJsonContentTypeHeader(processID) + getAcceptHeader() + getAuthorizationHeader();
}

OpResult StabilityClient::validateToken(const juce::String& inToken) const
{
    // Create the error here, in case we need it
    Error error;
    int statusCode = 0;

    juce::URL url = juce::URL("https://api.stability.ai/v1/user/account");

    // Create a GET request to account API with provided token 
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders("Authorization: Bearer " + token + "\r\n")
                       .withConnectionTimeoutMs(5000)
                       .withStatusCode(&statusCode);

    std::unique_ptr<juce::InputStream> stream(url.createInputStream(options));

    if (stream == nullptr)
    {
        error.code = statusCode;
        error.devMessage =
            "Failed to create input stream for GET request \nto validate token.";
        return OpResult::fail(error);
    }

    juce::String response = stream->readEntireStreamAsString();

    // Check the status code to ensure the request was successful
    if (statusCode != 200)
    {
        error.code = statusCode;
        error.devMessage =
            "Authentication failed with status code: " + juce::String(statusCode);
        return OpResult::fail(error);
    }

    // Parse the response
    juce::var parsedResponse = juce::JSON::parse(response);
    if (! parsedResponse.isObject())
    {
        error.devMessage = "Failed to parse JSON response from stability account API.";
        return OpResult::fail(error);
    }

    juce::DynamicObject* obj = parsedResponse.getDynamicObject();
    if (obj == nullptr)
    {
        error.devMessage = "Parsed JSON is not an object from stability account API.";
        return OpResult::fail(error);
    }

    return OpResult::ok();
}

void StabilityClient::setToken(const juce::String& inToken) { this->token = inToken; }

juce::String StabilityClient::getToken() const { return token; }

void StabilityClient::setTokenEnabled(bool enabled) { tokenEnabled = enabled; }

OpResult StabilityClient::buildPayload(const juce::String& prompt,
    const juce::String& processID,
    juce::String& payload) const
{
    juce::String boundary = "--------" + processID + "--------";
    payload = "--" + boundary + "\r\n";
    payload += "Content-Disposition: form-data; name=\"prompt\"\r\n\r\n";
    payload += prompt + "\r\n";
    payload += "--" + boundary + "\r\n";
    payload += "Content-Disposition: form-data; name=\"output_format\"\r\n\r\n";
    payload += "wav\r\n";
    payload += "--" + boundary + "\r\n";
    payload += "Content-Disposition: form-data; name=\"duration\"\r\n\r\n";
    payload += "30\r\n";
    payload += "--" + boundary + "\r\n";
    payload += "Content-Disposition: form-data; name=\"steps\"\r\n\r\n";
    payload += "30\r\n";
    payload += "--" + boundary + "--\r\n";
    return OpResult::ok();
}