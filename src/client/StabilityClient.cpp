#include "StabilityClient.h"
#include "../errors.h"
#include "../external/magic_enum.hpp"

StabilityClient::StabilityClient()
{
    tokenValidationURL = URL("https://api.stability.ai/v1/user/account");
}

String StabilityClient::mimeForAudioFile(const File& f)
{
    auto ext = f.getFileExtension().toLowerCase(); // includes the dot
    if (ext == ".wav" || ext == ".wave")
        return "audio/wav";
    if (ext == ".mp3")
        return "audio/mpeg";
    return {};
}

OpResult StabilityClient::setSpaceInfo(const SpaceInfo& inSpaceInfo)
{
    spaceInfo = inSpaceInfo;

    if (spaceInfo.status != SpaceInfo::Status::STABILITY)
    {
        return OpResult::fail(
            Error { ErrorType::InvalidURL, -1, "Invalid space info for StabilityClient" });
    }
    if (! spaceInfo.apiEndpointURL.isNotEmpty())
    {
        return OpResult::fail(Error { ErrorType::InvalidURL, -1, "API endpoint URL is empty" });
    }
    return OpResult::ok();
}

OpResult StabilityClient::uploadFileRequest(const File& fileToUpload,
                                            String& uploadedFilePath,
                                            const int timeoutMs) const
{
    // TBD. We need the original path of the file.

    if (! fileToUpload.existsAsFile())
    {
        Error error;
        error.devMessage = "File does not exist: " + fileToUpload.getFullPathName();
        return OpResult::fail(error);
    }

    uploadedFilePath = fileToUpload.getFullPathName();

    DBG("uploadFileRequest: returning uploadedFilePath = " + uploadedFilePath);
    return OpResult::ok();
}

String StabilityClient::getControlValue(const String& label, const Array<var>* dataArray)
{
    DBG("[getControlValue] Searching for label: " + label);
    for (const auto& item : *dataArray)
    {
        if (! item.isObject())
            continue;

        const auto itemStr = JSON::toString(item);
        DBG("[getControlValue] Scanning item: " + itemStr);

        if (item.hasProperty("label") && item.hasProperty("value"))
        {
            String itemLabel = item["label"].toString();
            if (itemLabel == label)
            {
                String value = item["value"].toString();
                DBG("[getControlValue] Found label: " + itemLabel + " => value: " + value);
                return value;
            }
        }
    }
    DBG("[getControlValue] Label not found: " + label);
    return {};
}

OpResult StabilityClient::processTextToAudio(const Array<var>* dataArray,
                                             Error& error,
                                             std::vector<String>& outputFilePaths)
{
    String processID = Uuid().toString();
    shouldCancel.store(false); // Reset cancel flag

    String prompt = "happy";
    String duration = "30";
    String steps = "30";
    String outputFormat = "wav";
    String cfg = "3.0";

    for (const auto& item : *dataArray)
    {
        auto* obj = item.getDynamicObject();
        if (! obj)
            continue;

        const auto label = obj->getProperty("label").toString().toLowerCase();
        const auto value = obj->getProperty("value").toString();

        if (label.contains("prompt"))
            prompt = value;
        else if (label.contains("duration"))
            duration = value;
        else if (label.contains("steps"))
            steps = value;
        else if (label.contains("cfg"))
            cfg = value;
        else if (label.contains("format"))
            outputFormat = value;
    }

    StringPairArray args;
    args.set("prompt", prompt);
    args.set("duration", duration);
    args.set("steps", steps);
    args.set("cfg", cfg);
    args.set("output_format", outputFormat);

    String payload;
    OpResult result = buildPayload(args, processID, payload);
    if (result.failed())
        return result;

    URL url("https://api.stability.ai/v2beta/audio/stable-audio-2/text-to-audio");
    url = url.withPOSTData(payload);

    const String headers = createJsonHeaders(processID);
    int statusCode = 0;
    StringPairArray responseHeaders;

    auto options = URL::InputStreamOptions(URL::ParameterHandling::inPostData)
                       .withExtraHeaders(headers)
                       .withResponseHeaders(&responseHeaders)
                       .withStatusCode(&statusCode)
                       .withHttpRequestCmd("POST")
                       .withConnectionTimeoutMs(30000);

    std::unique_ptr<InputStream> stream = url.createInputStream(options);

    if (shouldCancel.load())
    {
        error.devMessage = "Cancelled before receiving response.";
        return OpResult::fail(error);
    }

    if (! stream || statusCode != 200)
    {
        String response = stream ? stream->readEntireStreamAsString() : "";
        error.code = statusCode;
        if (response.contains("authorization"))
        {
            error.devMessage =
                "Missing or invalid authorization.\nHave you added a Stability AI access token in settings yet?";
        }
        else
        {
            error.devMessage = "Text-to-audio request failed: " + response;
        }

        return OpResult::fail(error);
    }

    File out = File::getSpecialLocation(File::tempDirectory)
                   .getChildFile(Uuid().toString() + "." + outputFormat);

    std::unique_ptr<FileOutputStream> f(out.createOutputStream());
    if (! f || ! f->openedOk())
    {
        error.devMessage = "Failed to write output file.";
        return OpResult::fail(error);
    }

    constexpr int bufSz = 16384;
    HeapBlock<char> buf(bufSz);

    for (;;)
    {
        if (shouldCancel.load())
        {
            error.devMessage = "Cancelled while downloading audio.";
            return OpResult::fail(error);
        }

        const int n = stream->read(buf.getData(), bufSz);
        if (n <= 0)
            break;
        f->write(buf.getData(), (size_t) n);
    }

    f->flush();
    outputFilePaths.push_back(URL(out).toString(true));
    return OpResult::ok();
}

OpResult StabilityClient::processAudioToAudio(const Array<var>* dataArray,
                                              Error& error,
                                              std::vector<String>& outputFilePaths)
{
    shouldCancel.store(false); // reset cancel flag

    if (dataArray == nullptr || dataArray->isEmpty())
    {
        error.devMessage = "dataArray is null or empty in processAudioToAudio.";
        return OpResult::fail(error);
    }

    // --- Step 1: Extract input audio path (index 0) ---
    String inputAudioPath;
    if (auto* obj = dataArray->getReference(0).getDynamicObject())
    {
        if (obj->hasProperty("value"))
        {
            var value = obj->getProperty("value");
            if (value.isObject())
            {
                DynamicObject* valueObj = value.getDynamicObject();
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

    File inputFile(inputAudioPath);
    if (! inputFile.existsAsFile())
    {
        error.devMessage = "Audio file does not exist: " + inputAudioPath;
        return OpResult::fail(error);
    }

    // --- Step 2: Extract controls by index ---
    auto getVal = [&](int idx) -> var
    {
        if (idx >= dataArray->size())
            return {};
        auto* obj = dataArray->getReference(idx).getDynamicObject();
        if (obj && obj->hasProperty("value"))
            return obj->getProperty("value");
        return {};
    };

    String duration = String((double) getVal(1));
    String steps = String((double) getVal(2));
    String cfg_scale = String((double) getVal(3));
    String outputFormat = getVal(4).toString();
    String prompt = getVal(5).toString();

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
    const String boundary = "--------------------------" + Uuid().toString().replace("-", "");
    MemoryOutputStream body;
    auto crlf = [&](const String& s) { body << s << "\r\n"; };

    // Audio file part
    //crlf("--" + boundary);
    //crlf("Content-Disposition: form-data; name=\"audio\"; filename=\"input.wav\"");
    //crlf("Content-Type: audio/wav");
    //crlf("");
    // --- Determine content type & use original filename ---
    const String contentType = mimeForAudioFile(inputFile);
    if (contentType.isEmpty())
    {
        error.devMessage = "Unsupported audio format. Please use WAV or MP3.";
        return OpResult::fail(error);
    }
    const String filename = inputFile.getFileName();

    // Audio file part
    crlf("--" + boundary);
    crlf("Content-Disposition: form-data; name=\"audio\"; filename=\"" + filename + "\"");
    crlf("Content-Type: " + contentType);
    crlf("");

    std::unique_ptr<FileInputStream> in(inputFile.createInputStream());
    if (! in || ! in->openedOk())
    {
        error.devMessage = "Failed to open input WAV for reading.";
        return OpResult::fail(error);
    }

    constexpr int bufSz = 16384;
    HeapBlock<char> buf(bufSz);
    for (;;)
    {
        const int n = in->read(buf.getData(), bufSz);
        if (n <= 0)
            break;
        body.write(buf.getData(), (size_t) n);
    }
    crlf(""); // End of file part

    // Helper to add text fields
    auto addField = [&](const String& key, const String& val)
    {
        if (val.isNotEmpty())
        {
            crlf("--" + boundary);
            crlf("Content-Disposition: form-data; name=\"" + key + "\"");
            crlf("");
            crlf(val);
        }
    };

    addField("prompt", prompt);
    addField("duration", duration);
    addField("steps", steps);
    addField("cfg_scale", cfg_scale);
    addField("output_format", outputFormat);

    // Final boundary
    crlf("--" + boundary + "--");

    // --- Step 4: Build and send POST request ---
    MemoryBlock blob = body.getMemoryBlock();
    //DBG("Multipart preview (first 512 bytes):\n" +
    //String::fromUTF8((const char*)blob.getData(), (int)jmin(blob.getSize(), (size_t)512)));

    URL url("https://api.stability.ai/v2beta/audio/stable-audio-2/audio-to-audio");
    url = url.withPOSTData(blob);

    const String headers = "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
                           + getAuthorizationHeader() + getAcceptHeader();

    int statusCode = 0;
    StringPairArray responseHeaders;

    auto opts = URL::InputStreamOptions(URL::ParameterHandling::inPostData)
                    .withHttpRequestCmd("POST")
                    .withExtraHeaders(headers)
                    .withResponseHeaders(&responseHeaders)
                    .withStatusCode(&statusCode)
                    .withConnectionTimeoutMs(60000);

    if (shouldCancel.load())
    {
        error.devMessage = "Cancelled before sending request.";
        return OpResult::fail(error);
    }

    std::unique_ptr<InputStream> stream = url.createInputStream(opts);

    if (! stream)
    {
        error.code = statusCode;
        error.devMessage = "Failed to create input stream for audio-to-audio POST.";
        return OpResult::fail(error);
    }

    if (statusCode != 200)
    {
        String response = stream ? stream->readEntireStreamAsString() : "";
        error.code = statusCode;

        if (response.contains("authorization"))
        {
            error.devMessage =
                "Invalid authorization.\nHave you added a Stability AI access token in settings yet?";
        }
        else
        {
            error.devMessage = "Audio-to-audio request failed: " + response;
        }

        return OpResult::fail(error);
    }

    // --- Step 5: Save returned audio to temp file ---
    // File out = File::getSpecialLocation(File::tempDirectory)
    //           .getChildFile(Uuid().toString() + ".wav");
    String outExt = outputFormat.isNotEmpty() ? "." + outputFormat : ".wav";
    File out =
        File::getSpecialLocation(File::tempDirectory).getChildFile(Uuid().toString() + outExt);

    std::unique_ptr<FileOutputStream> f(out.createOutputStream());

    if (! f || ! f->openedOk())
    {
        error.devMessage = "Failed to write output audio: " + out.getFullPathName();
        return OpResult::fail(error);
    }

    for (;;)
    {
        if (shouldCancel.load())
        {
            error.devMessage = "Cancelled while downloading output audio.";
            return OpResult::fail(error);
        }

        const int n = stream->read(buf.getData(), bufSz);
        if (n <= 0)
            break;
        f->write(buf.getData(), (size_t) n);
    }
    f->flush();

    outputFilePaths.push_back(URL(out).toString(true));
    return OpResult::ok();
}

OpResult StabilityClient::processRequest(Error& error,
                                         String& processingPayload,
                                         std::vector<String>& outputFilePaths,
                                         LabelList& labels)
{
    OpResult result = OpResult::ok();

    // Parse the processingPayload JSON
    var parsedPayload;
    JSON::parse(processingPayload, parsedPayload);

    if (! parsedPayload.isObject())
    {
        error.devMessage = "Failed to parse processingPayload. Not a JSON object.";
        return OpResult::fail(error);
    }

    DynamicObject* obj = parsedPayload.getDynamicObject();
    if (obj == nullptr)
    {
        error.devMessage = "Failed to get dynamic object from processingPayload.";
        return OpResult::fail(error);
    }

    Array<var>* dataArray = obj->getProperty("data").getArray();
    if (dataArray == nullptr || dataArray->isEmpty())
    {
        error.devMessage = "Missing or empty 'data' array in processingPayload.";
        return OpResult::fail(error);
    }
    //DBG("==== BEGIN: dataArray contents ====");
    //for (const auto& item : *dataArray)
    //{
    //   DBG(JSON::toString(item));
    //}
    //DBG("==== END: dataArray contents ====");

    // Dispatch to correct model endpoint
    const String modelName = spaceInfo.modelName.trim().toLowerCase();

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

OpResult StabilityClient::getControls(Array<var>& inputComponents,
                                      Array<var>& outputComponents,
                                      DynamicObject& cardDict)
{
    String callID = "controls";
    String eventID;

    // Initialize a positive result
    OpResult result = OpResult::ok();

    // Create an Error object in case we need it for the next steps
    Error error;
    error.type = ErrorType::JsonParseError;

    // Fetch the controls JSON. It's an array of 2 dicts.
    // The first is for text-to-audio, the second is for audio-to-audio.

    const juce::String controlsJsonUrl =
        "https://gist.githubusercontent.com/xribene/eb0650de86fdcf8d7324ded49e07bce9/raw/acd103b0bc6af3ef6f20802008da6f2a3ba7fc78/gistfile1.txt";

    URL url(controlsJsonUrl);
    String responseData = url.readEntireTextStream();

    if (responseData.isEmpty())
    {
        error.devMessage = "Failed to fetch controls JSON from URL: " + controlsJsonUrl;
        if (url.isWellFormed()
            && url.getDomain()
                   .isEmpty()) // Basic check if it looks like a local file path that failed
        {
            error.devMessage += ". Ensure the URL is correct and accessible.";
        }
        return OpResult::fail(error);
    }

    // Parse the extracted JSON string
    var parsedData;
    JSON::parse(responseData, parsedData);

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
    Array<var>* dataArray = parsedData.getArray();
    if (dataArray == nullptr)
    {
        error.devMessage = "Parsed JSON is not an array 2.";
        return OpResult::fail(error);
    }

    DynamicObject* obj = nullptr;
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
    DynamicObject* cardObj = obj->getProperty("card").getDynamicObject();

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

    Array<var>* inputsArray = obj->getProperty("inputs").getArray();
    if (inputsArray == nullptr)
    {
        error.devMessage = "Couldn't load the controls array/list from the controls response.";
        return OpResult::fail(error);
    }
    inputComponents = *inputsArray;

    Array<var>* outputsArray = obj->getProperty("outputs").getArray();
    if (outputsArray == nullptr)
    {
        error.devMessage = "Couldn't load the controls array/list from the controls response.";
        return OpResult::fail(error);
    }
    outputComponents = *outputsArray;

    return result;
}

OpResult StabilityClient::cancel()
{
    DBG("[StabilityClient] Cancel request received.");
    shouldCancel.store(true);
    return OpResult::ok();
}

String StabilityClient::getJsonContentTypeHeader(const String& processID) const
{
    String boundary = "--------" + processID + "--------";
    return "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
}

String StabilityClient::getAcceptHeader() const { return "Accept: audio/*,application/json\r\n"; }

String StabilityClient::createJsonHeaders(const String& processID) const
{
    return getJsonContentTypeHeader(processID) + getAcceptHeader() + getAuthorizationHeader();
}

OpResult
    StabilityClient::buildPayload(StringPairArray& args, String& processID, String& payload) const
{
    String boundary = "--------" + processID + "--------";
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
