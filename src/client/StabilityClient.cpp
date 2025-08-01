#include "StabilityClient.h"
#include "../errors.h"
#include "../external/magic_enum.hpp"


OpResult StabilityClient::setSpaceInfo(const SpaceInfo& inSpaceInfo)
{
    spaceInfo = inSpaceInfo;
    if (spaceInfo.status != SpaceInfo::Status::STABILITY)
    {
        return OpResult::fail(Error{ErrorType::InvalidURL, -1, "Invalid space info for StabilityClient"});
    }
    if (! spaceInfo.apiEndpointURL.isNotEmpty())
    {
        return OpResult::fail(Error{ErrorType::InvalidURL, -1, "API endpoint URL is empty"});
    }
    return OpResult::ok();
}

SpaceInfo StabilityClient::getSpaceInfo() const { return spaceInfo; }

OpResult StabilityClient::uploadFileRequest(const juce::File& fileToUpload,
                                         juce::String& uploadedFilePath,
                                         const int timeoutMs) const
{
    // TBD. We need the original path of the file.

    if (!fileToUpload.existsAsFile())
    {
        Error error;
        error.devMessage = "File does not exist: " + fileToUpload.getFullPathName();
        return OpResult::fail(error);
    }

    uploadedFilePath = fileToUpload.getFullPathName();

    DBG("uploadFileRequest: returning uploadedFilePath = " + uploadedFilePath);
    return OpResult::ok();
}

juce::String StabilityClient::getControlValue(const juce::String& label, const juce::Array<juce::var>* dataArray)
{
    DBG("[getControlValue] Searching for label: " + label);
    for (const auto& item : *dataArray)
    {
        if (!item.isObject()) continue;

        const auto itemStr = juce::JSON::toString(item);
        DBG("[getControlValue] Scanning item: " + itemStr);

        if (item.hasProperty("label") && item.hasProperty("value"))
        {
            juce::String itemLabel = item["label"].toString();
            if (itemLabel == label)
            {
                juce::String value = item["value"].toString();
                DBG("[getControlValue] Found label: " + itemLabel + " => value: " + value);
                return value;
            }
        }
    }
    DBG("[getControlValue] Label not found: " + label);
    return {};
}




OpResult StabilityClient::processTextToAudio(const juce::Array<juce::var>* dataArray,
                                             Error& error,
                                             std::vector<juce::String>& outputFilePaths)
{
    juce::String processID = juce::Uuid().toString();

    juce::String prompt = "happy";
    juce::String duration = "30";
    juce::String steps = "30";
    juce::String outputFormat = "wav";
    juce::String cfg = "3.0";

    for (const auto& item : *dataArray)
    {
        auto* obj = item.getDynamicObject();
        if (!obj) continue;

            const auto label = obj->getProperty("label").toString().toLowerCase();
            const auto value = obj->getProperty("value").toString();

        if (label.contains("prompt")) prompt = value;
            else if (label.contains("duration")) duration = value;
            else if (label.contains("steps")) steps = value;
            else if (label.contains("cfg")) cfg = value;
            else if (label.contains("format")) outputFormat = value;
    }

    juce::StringPairArray args;
    args.set("prompt", prompt);
    args.set("duration", duration);
    args.set("steps", steps);
    args.set("cfg", cfg);
    args.set("output_format", outputFormat);

    juce::String payload;
    OpResult result = buildPayload(args, processID, payload);
    if (result.failed()) return result;

    juce::URL url("https://api.stability.ai/v2beta/audio/stable-audio-2/text-to-audio");
    url = url.withPOSTData(payload);

    const juce::String headers = createJsonHeaders(processID);
    int statusCode = 0;
    juce::StringPairArray responseHeaders;

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                                 .withExtraHeaders(headers)
                                                 .withResponseHeaders(&responseHeaders)
                                                 .withStatusCode(&statusCode)
                                                 .withHttpRequestCmd("POST")
                                                 .withConnectionTimeoutMs(30000);

    std::unique_ptr<juce::InputStream> stream = url.createInputStream(options);
    if (!stream || statusCode != 200)
    {
        juce::String response = stream ? stream->readEntireStreamAsString() : "";
        error.code = statusCode;
        error.devMessage = "Text-to-audio request failed: " + response;
        return OpResult::fail(error);
    }

    juce::File out = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile(juce::Uuid().toString() + "." + outputFormat);

    std::unique_ptr<juce::FileOutputStream> f(out.createOutputStream());
    if (!f || !f->openedOk())
    {
        error.devMessage = "Failed to write output file.";
        return OpResult::fail(error);
    }

    f->writeFromInputStream(*stream, stream->getTotalLength());
    outputFilePaths.push_back(juce::URL(out).toString(true));
    return OpResult::ok();
}

OpResult StabilityClient::processAudioToAudio(const juce::Array<juce::var>* dataArray,
                                              Error& error,
                                              std::vector<juce::String>& outputFilePaths)
{
    if (dataArray == nullptr || dataArray->isEmpty())
    {
        error.devMessage = "dataArray is null or empty in processAudioToAudio.";
        return OpResult::fail(error);
    }

    // --- Step 1: Extract input audio path (index 0) ---
    juce::String inputAudioPath;
    if (auto* obj = dataArray->getReference(0).getDynamicObject())
    {
        if (obj->hasProperty("value"))
        {
            juce::var value = obj->getProperty("value");
            if (value.isObject())
            {
                juce::DynamicObject* valueObj = value.getDynamicObject();
                if (valueObj->hasProperty("path"))
                    inputAudioPath = valueObj->getProperty("path").toString();
            }
        }
    }

    if (inputAudioPath.isEmpty())
    {
        error.devMessage = "Missing or invalid inputAudioPath.";
        return OpResult::fail(error);
    }

    juce::File inputFile(inputAudioPath);
    if (!inputFile.existsAsFile())
    {
        error.devMessage = "Audio file does not exist: " + inputAudioPath;
        return OpResult::fail(error);
    }

    // --- Step 2: Extract controls by index ---
    auto getVal = [&](int idx) -> juce::var {
        if (idx >= dataArray->size()) return {};
        auto* obj = dataArray->getReference(idx).getDynamicObject();
        if (obj && obj->hasProperty("value"))
            return obj->getProperty("value");
        return {};
    };
    
    juce::String duration      = juce::String((double)getVal(1));
    juce::String steps         = juce::String((double)getVal(2));
    juce::String cfg_scale     = juce::String((double)getVal(3));
    juce::String outputFormat  = getVal(4).toString();
    juce::String prompt        = getVal(5).toString();
   

    //if (prompt.isEmpty())
    //  {
    //     DBG("WARNING: 'Text Prompt' is empty. Using fallback.");
    //     prompt = "cheerful acoustic track";
    //  }
    //  else
    //  {
    //   DBG("Resolved Prompt = " + prompt);
    //}

    // --- Step 3: Construct multipart body ---
    const juce::String boundary = "--------------------------" + juce::Uuid().toString().replace("-", "");
    juce::MemoryOutputStream body;
    auto crlf = [&](const juce::String& s) { body << s << "\r\n"; };

    // Audio file part
    crlf("--" + boundary);
    crlf("Content-Disposition: form-data; name=\"audio\"; filename=\"input.wav\"");
    crlf("Content-Type: audio/wav");
    crlf("");

    std::unique_ptr<juce::FileInputStream> in(inputFile.createInputStream());
    if (!in || !in->openedOk())
    {
        error.devMessage = "Failed to open input WAV for reading.";
        return OpResult::fail(error);
    }

    constexpr int bufSz = 16384;
    juce::HeapBlock<char> buf(bufSz);
    for (;;)
    {
        const int n = in->read(buf.getData(), bufSz);
        if (n <= 0) break;
        body.write(buf.getData(), (size_t)n);
    }
    crlf(""); // End of file part

    // Helper to add text fields
    auto addField = [&](const juce::String& key, const juce::String& val)
    {
    if (val.isNotEmpty())
    {
        crlf("--" + boundary);
        crlf("Content-Disposition: form-data; name=\"" + key + "\"");
        crlf("");
        crlf(val);
    }
    };

    addField("prompt",        prompt);
    addField("duration",      duration);
    addField("steps",         steps);
    addField("cfg_scale",     cfg_scale);
    addField("output_format", outputFormat);

    // Final boundary
    crlf("--" + boundary + "--");

    // --- Step 4: Build and send POST request ---
    juce::MemoryBlock blob = body.getMemoryBlock();
    //DBG("Multipart preview (first 512 bytes):\n" +
    //juce::String::fromUTF8((const char*)blob.getData(), (int)juce::jmin(blob.getSize(), (size_t)512)));

    juce::URL url("https://api.stability.ai/v2beta/audio/stable-audio-2/audio-to-audio");
    url = url.withPOSTData(blob);

    const juce::String headers = "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n" + getAuthorizationHeader() + getAcceptHeader();

    int statusCode = 0;
    juce::StringPairArray responseHeaders;

    auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                              .withHttpRequestCmd("POST")
                                              .withExtraHeaders(headers)
                                              .withResponseHeaders(&responseHeaders)
                                              .withStatusCode(&statusCode)
                                              .withConnectionTimeoutMs(60000);

    std::unique_ptr<juce::InputStream> stream = url.createInputStream(opts);

    if (!stream)
    {
        error.code = statusCode;
        error.devMessage = "Failed to create input stream for audio-to-audio POST.";
        return OpResult::fail(error);
    }

    if (statusCode != 200)
    {
        error.code = statusCode;
        error.devMessage = "Audio-to-audio request failed: " + stream->readEntireStreamAsString();
        return OpResult::fail(error);
    }

    // --- Step 5: Save returned audio to temp file ---
    juce::File out = juce::File::getSpecialLocation(juce::File::tempDirectory)
    .getChildFile(juce::Uuid().toString() + ".wav");
    std::unique_ptr<juce::FileOutputStream> f(out.createOutputStream());

    if (!f || !f->openedOk())
    {
        error.devMessage = "Failed to write output audio: " + out.getFullPathName();
        return OpResult::fail(error);
    }

    for (;;)
    {
        const int n = stream->read(buf.getData(), bufSz);
        if (n <= 0) break;
        f->write(buf.getData(), (size_t)n);
    }
    f->flush();

    outputFilePaths.push_back(juce::URL(out).toString(true));
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
    //DBG("==== BEGIN: dataArray contents ====");
    //for (const auto& item : *dataArray)
    //{
    //   DBG(juce::JSON::toString(item));
    //}
    //DBG("==== END: dataArray contents ====");

    // Dispatch to correct model endpoint
    const juce::String modelName = spaceInfo.modelName.trim().toLowerCase();

    if (modelName == "audio-to-audio" || modelName == "stability/audio-to-audio")
    {
        return processAudioToAudio(dataArray, error, outputFilePaths);
    }
    else if (modelName == "text-to-audio" || modelName == "stability/text-to-audio")
    {
        return processTextToAudio(dataArray, error, outputFilePaths);
    }
    else
    {
        error.devMessage = "Unsupported Stability AI model: " + modelName;
        return OpResult::fail(error);
    }
}



OpResult StabilityClient::getControls(juce::Array<juce::var>& inputComponents,
                                      juce::Array<juce::var>& outputComponents,
                                      juce::DynamicObject& cardDict)
{
    juce::String callID = "controls";
    juce::String eventID;

    // Initialize a positive result
    OpResult result = OpResult::ok();

    // Create an Error object in case we need it for the next steps
    Error error;
    error.type = ErrorType::JsonParseError;

    // Fetch the controls JSON from the URL. It's an array of 2 dicts.
    // The first is for text-to-audio, the second is for audio-to-audio.
    const juce::String controlsJsonUrl = "https://gist.githubusercontent.com/xribene/eb0650de86fdcf8d7324ded49e07bce9/raw/acd103b0bc6af3ef6f20802008da6f2a3ba7fc78/gistfile1.txt";

    juce::URL url(controlsJsonUrl);
    juce::String responseData = url.readEntireTextStream();

    if (responseData.isEmpty())
    {
        error.devMessage = "Failed to fetch controls JSON from URL: " + controlsJsonUrl;
        if (url.isWellFormed() && url.getDomain().isEmpty()) // Basic check if it looks like a local file path that failed
        {
            error.devMessage += ". Ensure the URL is correct and accessible.";
        }
        return OpResult::fail(error);
    }

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

    juce::DynamicObject* obj = nullptr;
    if (spaceInfo.modelName == "text-to-audio")
    // Check if the first element in the array is a dict
    obj = dataArray->getReference(0).getDynamicObject();
    else if (spaceInfo.modelName == "audio-to-audio")
    // Check if the first element in the array is a dict
    obj = dataArray->getReference(1).getDynamicObject();

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

OpResult StabilityClient::buildPayload(juce::StringPairArray& args, juce::String& processID, juce::String& payload) const {
    juce::String boundary = "--------" + processID + "--------";
    payload = "--" + boundary + "\r\n";
    payload += "Content-Disposition: form-data; name=\"prompt\"\r\n\r\n";
    payload += args["prompt"] + "\r\n";

    payload += "--" + boundary + "\r\n";
    payload += "Content-Disposition: form-data; name=\"output_format\"\r\n\r\n";
    payload += args["output_format"] + "\r\n";

    payload += "--" + boundary + "\r\n";
    payload += "Content-Disposition: form-data; name=\"duration\"\r\n\r\n";
    payload += args["duration"] + "\r\n";

    payload += "--" + boundary + "\r\n";
    payload += "Content-Disposition: form-data; name=\"steps\"\r\n\r\n";
    payload += args["steps"] + "\r\n";

    payload += "--" + boundary + "\r\n";
    payload += "Content-Disposition: form-data; name=\"cfg\"\r\n\r\n";
    payload += args["cfg"] + "\r\n";

    payload += "--" + boundary + "--\r\n";

    return OpResult::ok();
}
