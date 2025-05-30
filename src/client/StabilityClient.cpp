#include "StabilityClient.h"
#include "../errors.h"
#include "../external/magic_enum.hpp"


OpResult StabilityClient::setSpaceInfo(const SpaceInfo& inSpaceInfo)
{
    OpResult result = OpResult::ok();
    return result;
}

SpaceInfo StabilityClient::getSpaceInfo() const { return spaceInfo; }

OpResult StabilityClient::uploadFileRequest(const juce::File& fileToUpload,
                                         juce::String& uploadedFilePath,
                                         const int timeoutMs) const
{
    // TBD. We need the original path of the file.
    return OpResult::ok();
}

OpResult StabilityClient::processRequest(Error& error,
                                         juce::String& processingPayload,
                                         std::vector<juce::String>& outputFilePaths,
                                         LabelList& labels)
{
    OpResult result = OpResult::ok();
    juce::String processID = juce::Uuid().toString();
    juce::var parseData;
    juce::JSON::parse(processingPayload, parseData);
    juce::DynamicObject* obj = parseData.getDynamicObject();
    juce::Array<juce::var>* dataArray = obj->getProperty("data").getArray();
    juce::String prompt = dataArray->getReference(0).toString();
    // juce::String prompt = juce::String("3/4 time signature");

    juce::URL textToAudioUrl = juce::URL(juce::String("https://api.stability.ai/v2beta/audio/stable-audio-2/text-to-audio"));

    juce::String payload;
    result = buildPayload(prompt, processID, payload);

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

    if (statusCode != 200) // TODO: load the error message from the response
    {
        juce::String responseJson = stream -> readEntireStreamAsString();
        error.devMessage = "Request failed with status code: " + juce::String(statusCode) + ", " + responseJson;
        return OpResult::fail(error);
    }

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::String fileName = juce::Uuid().toString() + ".wav";
    juce::File downloadedFile = tempDir.getChildFile(fileName);

    std::unique_ptr<juce::FileOutputStream> fileOutput(downloadedFile.createOutputStream());

    if (fileOutput == nullptr || ! fileOutput->openedOk())
    {
        error.devMessage =
            "Failed to create output stream for file: " + downloadedFile.getFullPathName();
        return OpResult::fail(error);
    }

    // Copy data from the input stream to the output stream
    fileOutput->writeFromInputStream(*stream, stream->getTotalLength());

    outputFilePaths.push_back(URL(downloadedFile).toString(true));

    return result;
}

OpResult StabilityClient::getControls(juce::Array<juce::var>& inputComponents,
                                   juce::Array<juce::var>& outputComponents,
                                   juce::DynamicObject& cardDict)
{
    juce::String callID = "controls";
    juce::String eventID;

    // Initialize a positive result
    OpResult result = OpResult::ok();
    juce::String responseData = "[{\"card\": {\"name\": \"Text to Audio\", \"description\": \"Integrated stability text to audio\", \"author\": \"Stability\", \"tags\": [\"example\", \"stability\", \"test\"]}, \"inputs\": [{\"label\": \"Input Text Prompt\", \"value\": \"3/4 time signature\", \"type\": \"text_box\"}], \"outputs\": [{\"label\": \"Output Audio\", \"required\": true, \"type\": \"audio_track\"}]}]";

    // Create an Error object in case we need it for the next steps
    Error error;
    error.type = ErrorType::JsonParseError;
    // Parse the extracted JSON string
    juce::var parsedData;
    juce::JSON::parse(responseData, parsedData);

    if (! parsedData.isObject())
    {
        error.devMessage = "Failed to parse the data portion of the received controls JSON.";
        return OpResult::fail(error);
    }

    if (! parsedData.isArray())
    {
        error.devMessage = "Parsed JSON is not an array.";
        return OpResult::fail(error);
    }
    juce::Array<juce::var>* dataArray = parsedData.getArray();
    if (dataArray == nullptr)
    {
        error.devMessage = "Parsed JSON is not an array 2.";
        return OpResult::fail(error);
    }
    // Check if the first element in the array is a dict
    juce::DynamicObject* obj = dataArray->getFirst().getDynamicObject();
    if (obj == nullptr)
    {
        error.devMessage = "First element in the array is not a dict.";
        return OpResult::fail(error);
    }

    // Get the card and controls objects from the parsed data
    juce::DynamicObject* cardObj = obj->getProperty("card").getDynamicObject();

    if (cardObj == nullptr)
    {
        error.devMessage = "Couldn't load the modelCard dict from the controls response.";
        return OpResult::fail(error);
    }

    // Clear the existing properties in cardDict
    cardDict.clear();

    // Copy all properties from cardObj to cardDict
    for (auto& key : cardObj->getProperties())
    {
        cardDict.setProperty(key.name, key.value);
    }

    juce::Array<juce::var>* inputsArray = obj->getProperty("inputs").getArray();
    if (inputsArray == nullptr)
    {
        error.devMessage = "Couldn't load the controls array/list from the controls response.";
        return OpResult::fail(error);
    }
    inputComponents = *inputsArray;

    juce::Array<juce::var>* outputsArray = obj->getProperty("outputs").getArray();
    if (outputsArray == nullptr)
    {
        error.devMessage = "Couldn't load the controls array/list from the controls response.";
        return OpResult::fail(error);
    }
    outputComponents = *outputsArray;

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

juce::String StabilityClient::getJsonContentTypeHeader(juce::String& processID) const
{
    juce::String boundary = "--------" + processID + "--------";
    return "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
}

juce::String StabilityClient::getAcceptHeader() const { return "Accept: audio/*\r\n"; }

juce::String StabilityClient::createCommonHeaders() const
{
    return getAcceptHeader() + getAuthorizationHeader();
}

juce::String StabilityClient::createJsonHeaders(juce::String& processID) const
{
    return getJsonContentTypeHeader(processID) + getAcceptHeader() + getAuthorizationHeader();
}

OpResult StabilityClient::validateToken(const juce::String& token) const
{
    // Create the error here, in case we need it
    Error error;
    int statusCode = 0;

    juce::URL url = juce::URL("https://huggingface.co/api/whoami-v2");

    // Create a GET request to whoami-v2 API with provided token 
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
        error.devMessage = "Failed to parse JSON response from whoami-v2 API.";
        return OpResult::fail(error);
    }

    juce::DynamicObject* obj = parsedResponse.getDynamicObject();
    if (obj == nullptr)
    {
        error.devMessage = "Parsed JSON is not an object from whoami-v2 API.";
        return OpResult::fail(error);
    }

    auto* tokenJSON = obj->getProperty("auth").getDynamicObject()->getProperty("accessToken").getDynamicObject();

    String role = tokenJSON->getProperty("role").toString();

    if (!(role == "write" || role == "read"))
    {
        bool hasAllPermissions = false;

        auto* scopedArray = tokenJSON->getProperty("fineGrained").getDynamicObject()->getProperty("scoped").getArray();

        for (const auto& scopeEntry : *scopedArray)
        {
            if (!scopeEntry.isObject())
                continue;
    
            var permissionsVar = scopeEntry.getDynamicObject()->getProperty("permissions");
    
            if (!permissionsVar.isArray())
                continue;
    
            auto* permissionsArray = permissionsVar.getArray();
            bool hasAll = permissionsArray->contains("repo.content.read") &&
                          permissionsArray->contains("repo.write") &&
                          permissionsArray->contains("inference.serverless.write") &&
                          permissionsArray->contains("inference.endpoints.infer.write");
    
            if (hasAll)
            {
                hasAllPermissions = true;
                break;
            }
        }
    
        if (!hasAllPermissions)
        {
            error.devMessage = "Provided token does not have suitable read/write permissions.";
            return OpResult::fail(error);
        }
    }

    return OpResult::ok();
}

void StabilityClient::setToken(const juce::String& token) { this->token = token; }

juce::String StabilityClient::getToken() const { return token; }

void StabilityClient::setTokenEnabled(bool enabled) { tokenEnabled = enabled; }

OpResult StabilityClient::buildPayload(juce::String& prompt, juce::String& processID, juce::String& payload) const {
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
    /*
    juce::DynamicObject::Ptr dataObject = new juce::DynamicObject();
    dataObject->setProperty(juce::String("prompt"), juce::var(prompt));
    dataObject->setProperty(juce::String("output_format"), juce::var(juce::String("wav")));
    dataObject->setProperty(juce::String("duration"), juce::var(30));
    dataObject->setProperty(juce::String("steps"), juce::var(30));
    payload = juce::JSON::toString(juce::var(dataObject), true);
    payload.set("prompt", prompt);
    payload.set("outputformat", "wav");
    payload.set("duration", "30");
    payload.set("steps", "30");
    */
    return OpResult::ok();
}