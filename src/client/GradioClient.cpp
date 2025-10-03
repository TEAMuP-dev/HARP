#include "GradioClient.h"
#include "../errors.h"
#include "../external/magic_enum.hpp"

// Space Info
OpResult GradioClient::setSpaceInfo(const SpaceInfo& inSpaceInfo)
{
    spaceInfo = inSpaceInfo;
    return OpResult::ok();
}

SpaceInfo GradioClient::getSpaceInfo() const { return spaceInfo; }

// Requests
OpResult GradioClient::processRequest(Error& error,
                                      juce::String& processingPayload,
                                      std::vector<juce::String>& outputFilePaths,
                                      LabelList& labels) {
    OpResult result = OpResult::ok();
    juce::String eventId;
    juce::String endpoint = "process";
    result = makePostRequestForEventID(endpoint, eventId, processingPayload);
    if (result.failed())
    {
        result.getError().devMessage = "Failed to make post request.";
        return result;
    }

    juce::String response;
    result = getResponseFromEventID(endpoint, eventId, response, -1);
    if (result.failed())
    {
        result.getError().devMessage = "Failed to make get request";
        return result;
    }

    juce::String responseData;

    juce::String key = "data: ";
    result = extractKeyFromResponse(response, responseData, key);
    if (result.failed())
    {
        result.getError().devMessage = "Failed to extract 'data:'";
        return result;
    }

    juce::var parsedData;
    juce::JSON::parse(responseData, parsedData);
    if (! parsedData.isObject())
    {
        error.devMessage = "Failed to parse the 'data' key of the received JSON.";
        return OpResult::fail(error);
    }
    if (! parsedData.isArray())
    {
        error.devMessage = "Parsed data field should be an array.";
        return OpResult::fail(error);
    }

    juce::Array<juce::var>* dataArray = parsedData.getArray();
    if (dataArray == nullptr)
    {
        error.devMessage = "The data array is empty.";
        return OpResult::fail(error);
    }

    // Iterate through the array elements
    for (int i = 0; i < dataArray->size(); i++)
    {
        juce::var procObj = dataArray->getReference(i);
        if (! procObj.isObject())
        {
            error.devMessage =
                "The " + juce::String(i)
                + "th returned element of the process_fn function in the gradio-app is not an object.";
            return OpResult::fail(error);
        }
        // Make sure the object has a "meta" key
        // Gradio output components like File and Audio store metadata in the "meta" key
        // so we can use that to identify what kind of output it is
        if (! procObj.getDynamicObject())
        {
            error.devMessage =
                "The " + juce::String(i)
                + "th returned element of the process_fn function in the gradio-app is not a valid object. "
                + "Make sure you are using LabelList() and not just a python list, in process_fn, to return the output labels.";
            return OpResult::fail(error);
        }
        if (! procObj.getDynamicObject()->hasProperty("meta"))
        {
            error.type = ErrorType::MissingJsonKey;
            error.devMessage =
                "The " + juce::String(i)
                + "th element of the array of processed outputs does not have a meta object. "
                + "Make sure you are using LabelList() in process_fn to return the output labels.";
            return OpResult::fail(error);
        }
        juce::var meta = procObj.getDynamicObject()->getProperty("meta");
        // meta should be an object
        if (! meta.isObject())
        {
            error.type = ErrorType::MissingJsonKey;
            error.devMessage =
                "The " + juce::String(i)
                + "th element of the array of processed outputs does not have a valid meta object.";
            return OpResult::fail(error);
        }
        juce::String procObjType = meta.getDynamicObject()->getProperty("_type").toString();

        // procObjType could be "gradio.FileData" for file/midi/audio
        // and "pyharp.LabelList" for labels
        if (procObjType == "gradio.FileData")
        {
            juce::String outputFilePath;
            juce::String url = procObj.getDynamicObject()->getProperty("url").toString();

            result = downloadFileFromURL(url, outputFilePath);
            if (result.failed())
            {
                return result;
            }
            // Make a juce::File from the path
            juce::File downloadedFile(outputFilePath);
            // auto aa = downloadedFile.getFileName();
            // auto bb = downloadedFile.getFullPathName();
            // auto cc = URL(downloadedFile).toString(true);
            outputFilePaths.push_back(URL(downloadedFile).toString(true));
        }
        else if (procObjType == "pyharp.LabelList")
        {
            juce::Array<juce::var>* labelsPyharp =
                procObj.getDynamicObject()->getProperty("labels").getArray();

            for (int j = 0; j < labelsPyharp->size(); j++)
            {
                juce::DynamicObject* labelPyharp =
                    labelsPyharp->getReference(j).getDynamicObject();
                juce::String labelType = labelPyharp->getProperty("label_type").toString();
                std::unique_ptr<OutputLabel> label;

                if (labelType == "AudioLabel")
                {
                    auto audioLabel = std::make_unique<AudioLabel>();
                    if (labelPyharp->hasProperty("amplitude"))
                    {
                        if (labelPyharp->getProperty("amplitude").isDouble()
                            || labelPyharp->getProperty("amplitude").isInt())
                        {
                            audioLabel->amplitude =
                                static_cast<float>(labelPyharp->getProperty("amplitude"));
                        }
                    }
                    label = std::move(audioLabel);
                }
                else if (labelType == "SpectrogramLabel")
                {
                    auto spectrogramLabel = std::make_unique<SpectrogramLabel>();
                    if (labelPyharp->hasProperty("frequency"))
                    {
                        if (labelPyharp->getProperty("frequency").isDouble()
                            || labelPyharp->getProperty("frequency").isInt())
                        {
                            spectrogramLabel->frequency =
                                static_cast<float>(labelPyharp->getProperty("frequency"));
                        }
                    }
                    label = std::move(spectrogramLabel);
                }
                else if (labelType == "MidiLabel")
                {
                    auto midiLabel = std::make_unique<MidiLabel>();
                    if (labelPyharp->hasProperty("pitch"))
                    {
                        if (labelPyharp->getProperty("pitch").isDouble()
                            || labelPyharp->getProperty("pitch").isInt())
                        {
                            midiLabel->pitch =
                                static_cast<float>(labelPyharp->getProperty("pitch"));
                        }
                    }
                    label = std::move(midiLabel);
                }
                else if (labelType == "OutputLabel")
                {
                    auto outputLabel = std::make_unique<OutputLabel>();
                    label = std::move(outputLabel);
                }
                else
                {
                    error.type = ErrorType::UnknownLabelType;
                    error.devMessage = "Unknown label type: " + labelType;
                    return OpResult::fail(error);
                }
                // All the labels, no matter their type, have some common properties
                // t: float
                // label: str
                // duration: float = 0.0
                // description: str = None
                // color: int = 0
                // first we'll check which of those exist and are not void or null
                // for those that exist, we fill the struct properties
                // the rest will be ignored
                if (labelPyharp->hasProperty("t"))
                {
                    // now check if it's a float
                    if (labelPyharp->getProperty("t").isDouble()
                        || labelPyharp->getProperty("t").isInt())
                    {
                        label->t = static_cast<float>(labelPyharp->getProperty("t"));
                    }
                }
                if (labelPyharp->hasProperty("label"))
                {
                    // now check if it's a string
                    if (labelPyharp->getProperty("label").isString())
                    {
                        label->label = labelPyharp->getProperty("label").toString();
                        DBG("label: " + label->label);
                    }
                }
                if (labelPyharp->hasProperty("duration"))
                {
                    // now check if it's a float
                    if (labelPyharp->getProperty("duration").isDouble()
                        || labelPyharp->getProperty("duration").isInt())
                    {
                        label->duration =
                            static_cast<float>(labelPyharp->getProperty("duration"));
                    }
                }
                if (labelPyharp->hasProperty("description"))
                {
                    // now check if it's a string
                    if (labelPyharp->getProperty("description").isString())
                    {
                        label->description = labelPyharp->getProperty("description").toString();
                    }
                }
                if (labelPyharp->hasProperty("color"))
                {
                    // now check if it's an int
                    if ((labelPyharp->getProperty("color").isInt64()
                            || labelPyharp->getProperty("color").isInt()))
                    {
                        int color_val = static_cast<int>(labelPyharp->getProperty("color"));

                        if (color_val != 0)
                        {
                            label->color = color_val;
                        }
                    }
                }
                if (labelPyharp->hasProperty("link"))
                {
                    // now check if it's a string
                    if (labelPyharp->getProperty("link").isString())
                    {
                        label->link = labelPyharp->getProperty("link").toString();
                    }
                }
                labels.push_back(std::move(label));
            }
        }
        else
        {
            LogAndDBG("The pyharp Gradio app returned a " + procObjType
                        + " object, that we don't yet support in HARP.");
        }
    }
    return result;
}


OpResult GradioClient::extractKeyFromResponse(const juce::String& response,
                                              juce::String& responseKey,
                                              const juce::String& key) const
//   OpResult& result) const
//   juce::String& error) const

{
    int dataIndex = response.indexOf(key);

    if (dataIndex == -1)
    {
        Error error;
        error.type = ErrorType::MissingJsonKey;
        error.devMessage = "Missing Key in JSON :: Key " + key + " not found in response";
        return OpResult::fail(error);
    }

    // Extract the portion after "key: "
    responseKey = response.substring(dataIndex + key.length()).trim();
    return OpResult::ok();
}


OpResult GradioClient::uploadFileRequest(const juce::File& fileToUpload,
                                         juce::String& uploadedFilePath,
                                         const int timeoutMs) const
{
    juce::URL gradioEndpoint = spaceInfo.gradio;
    juce::URL uploadEndpoint = gradioEndpoint.getChildURL("gradio_api").getChildURL("upload");

    juce::StringPairArray responseHeaders;
    int statusCode = 0;
    juce::String mimeType = "audio/midi";

    // Create the error here, in case we need it
    // All the errors of this function are of type FileUploadError
    Error error;
    error.type = ErrorType::FileUploadError;

    // Use withFileToUpload to handle the multipart/form-data construction
    auto postEndpoint = uploadEndpoint.withFileToUpload("files", fileToUpload, mimeType);

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                       .withExtraHeaders(createCommonHeaders())
                       .withConnectionTimeoutMs(timeoutMs)
                       .withResponseHeaders(&responseHeaders)
                       .withStatusCode(&statusCode)
                       .withNumRedirectsToFollow(5)
                       .withHttpRequestCmd("POST");

    // Create the input stream for the POST request
    std::unique_ptr<juce::InputStream> stream(postEndpoint.createInputStream(options));

    if (stream == nullptr)
    {
        error.code = statusCode;
        error.devMessage = "Failed to create input stream for file upload request.";
        return OpResult::fail(error);
    }

    juce::String response = stream->readEntireStreamAsString();

    // Check the status code to ensure the request was successful
    if (statusCode != 200)
    {
        error.devMessage = "Request failed with status code: " + juce::String(statusCode);
        return OpResult::fail(error);
    }

    // Parse the response
    juce::var parsedResponse = juce::JSON::parse(response);
    if (! parsedResponse.isObject())
    {
        error.devMessage = "Failed to parse JSON response.";
        return OpResult::fail(error);
    }

    juce::Array<juce::var>* responseArray = parsedResponse.getArray();
    if (responseArray == nullptr || responseArray->isEmpty())
    {
        error.devMessage = "Parsed JSON does not contain the expected array.";
        return OpResult::fail(error);
    }

    // Get the first element in the array, which is the file path
    uploadedFilePath = responseArray->getFirst().toString();
    if (uploadedFilePath.isEmpty())
    {
        error.devMessage = "File path is empty in the response.";
        return OpResult::fail(error);
    }

    // DBG("File uploaded successfully, path: " + uploadedFilePath);
    return OpResult::ok();
}

OpResult GradioClient::makePostRequestForEventID(const juce::String endpoint,
                                                 juce::String& eventID,
                                                 const juce::String jsonBody,
                                                 const int timeoutMs) const
{
    // Create the error here, in case we need it
    // All the errors of this function are of type FileUploadError
    Error error;
    error.type = ErrorType::HttpRequestError;

    // Ensure that setSpaceInfo has been called before this method
    juce::URL gradioEndpoint = spaceInfo.gradio;
    juce::URL requestEndpoint =
        gradioEndpoint.getChildURL("gradio_api").getChildURL("call").getChildURL(endpoint);

    // Prepare the POST request
    // juce::String jsonBody = R"({"data": []})";
    juce::URL postEndpoint = requestEndpoint.withPOSTData(jsonBody);
    juce::StringPairArray responseHeaders;
    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                       .withExtraHeaders(createJsonHeaders())
                       .withConnectionTimeoutMs(timeoutMs)
                       .withResponseHeaders(&responseHeaders)
                       .withStatusCode(&statusCode)
                       .withNumRedirectsToFollow(5)
                       .withHttpRequestCmd("POST");

    // Create the input stream for the POST request
    std::unique_ptr<juce::InputStream> stream(postEndpoint.createInputStream(options));

    if (stream == nullptr)
    {
        error.code = statusCode;
        error.devMessage = "Failed to create input stream for POST request to " + endpoint;
        return OpResult::fail(error);
    }

    juce::String response = stream->readEntireStreamAsString();

    // Check the status code to ensure the request was successful
    if (statusCode != 200)
    {
        error.code = statusCode;
        error.devMessage =
            "Request to " + endpoint + " failed with status code: " + juce::String(statusCode);
        return OpResult::fail(error);
    }

    // Parse the response
    juce::var parsedResponse = juce::JSON::parse(response);
    if (! parsedResponse.isObject())
    {
        error.devMessage = "Failed to parse JSON response from " + endpoint;
        return OpResult::fail(error);
    }

    juce::DynamicObject* obj = parsedResponse.getDynamicObject();
    if (obj == nullptr)
    {
        error.devMessage = "Parsed JSON is not an object from " + endpoint;
        return OpResult::fail(error);
    }

    eventID = obj->getProperty("event_id");
    if (eventID.isEmpty())
    {
        error.type = ErrorType::MissingJsonKey;
        error.devMessage = "event_id not found in the response from " + endpoint;
        return OpResult::fail(error);
    }

    DBG(eventID);

    return OpResult::ok();
}

OpResult GradioClient::getResponseFromEventID(const juce::String callID,
                                              const juce::String eventID,
                                              juce::String& response,
                                              const int timeoutMs) const
{
    // Create the error here, in case we need it
    Error error;
    error.type = ErrorType::HttpRequestError;

    // Now we make a GET request to the endpoint with the event ID appended
    // The endpoint for the get request is the same as the post request with
    // /{eventID} appended
    juce::URL gradioEndpoint = spaceInfo.gradio;
    juce::URL getEndpoint = gradioEndpoint.getChildURL("gradio_api")
                                .getChildURL("call")
                                .getChildURL(callID)
                                .getChildURL(eventID);
    juce::StringPairArray responseHeaders;
    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders(createCommonHeaders())
                       .withConnectionTimeoutMs(timeoutMs)
                       .withResponseHeaders(&responseHeaders)
                       .withStatusCode(&statusCode)
                       .withNumRedirectsToFollow(5);
    //  .withHttpRequestCmd ("POST");
    std::unique_ptr<juce::InputStream> stream(getEndpoint.createInputStream(options));
    DBG("Input stream created");

    if (stream == nullptr)
    {
        error.code = statusCode;
        error.devMessage =
            "Failed to create input stream for GET request \nto " + callID + "/" + eventID;
        return OpResult::fail(error);
    }

    // Stream the response
    while (! stream->isExhausted())
    {
        response = stream->readNextLine();

        DBG(eventID);
        DBG(response);
        DBG(response.length());

        if (response.contains(enumToString(GradioEvents::complete)))
        {
            response = stream->readNextLine();
            break;
        }
        else if (response.contains(enumToString(GradioEvents::error)))
        {
            response = stream->readNextLine();
            error.code = statusCode;
            error.devMessage = response;
            return OpResult::fail(error);
        }
    }
    return OpResult::ok();
}

OpResult GradioClient::getControls(juce::Array<juce::var>& inputComponents,
                                   juce::Array<juce::var>& outputComponents,
                                   juce::DynamicObject& cardDict)
{
    juce::String callID = "controls";
    juce::String eventID;

    // Initialize a positive result
    OpResult result = OpResult::ok();

    result = makePostRequestForEventID(callID, eventID);
    if (result.failed())
    {
        return result;
    }

    juce::String response;
    result = getResponseFromEventID(callID, eventID, response);
    if (result.failed())
    {
        return result;
    }

    // From the gradio app, we receive a JSON string
    // (see core.py in pyharp --> gr.Text(label="Controls"))
    // Extract the data portion from the response
    juce::String responseData;
    result = extractKeyFromResponse(response, responseData, "data: ");
    if (result.failed())
    {
        return result;
    }

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

OpResult GradioClient::cancel() {
    OpResult result = OpResult::ok();
    juce::String eventId;
    juce::String endpoint = "cancel";

    // Perform a POST request to the cancel endpoint to get the event ID
    juce::String jsonBody = R"({"data": []})"; // The body is empty in this case

    result = makePostRequestForEventID(endpoint, eventId, jsonBody);
    if (result.failed())
    {
        return result;
    }

    // Use the event ID to make a GET request for the cancel response
    juce::String response;
    result = getResponseFromEventID(endpoint, eventId, response);
    return result;
}

OpResult GradioClient::downloadFileFromURL(const juce::URL& fileURL,
                                           juce::String& downloadedFilePath,
                                           const int timeoutMs) const
{
    // Create the error here, in case we need it
    Error error;
    error.type = ErrorType::FileDownloadError;

    // Determine the local temporary directory for storing the downloaded file
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
    juce::String fileName = fileURL.getFileName();
    // // Add a timestamp to the file name to avoid overwriting
    // // Insert timestamp before the file extension using juce::File operations
    // juce::String baseName = juce::File::createFileWithoutCheckingPath(fileName).getFileNameWithoutExtension();
    // juce::String extension = juce::File::createFileWithoutCheckingPath(fileName).getFileExtension();
    // juce::String timestamp = "_" + juce::String(juce::Time::getCurrentTime().formatted("%Y%m%d%H%M%S"));
    // fileName = baseName + timestamp + extension;
    juce::String baseName = juce::File::createFileWithoutCheckingPath(fileName).getFileNameWithoutExtension();
    juce::String extension = juce::File::createFileWithoutCheckingPath(fileName).getFileExtension();
    juce::File downloadedFile = tempDir.getChildFile(baseName + "_" + Uuid().toString() + extension);

    // Create input stream to download the file
    juce::StringPairArray responseHeaders;
    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders(createCommonHeaders())
                       .withConnectionTimeoutMs(timeoutMs)
                       .withResponseHeaders(&responseHeaders)
                       .withStatusCode(&statusCode)
                       .withNumRedirectsToFollow(5);

    std::unique_ptr<juce::InputStream> stream(fileURL.createInputStream(options));

    if (stream == nullptr)
    {
        error.devMessage = "Failed to create input stream for file download request.";
        return OpResult::fail(error);
    }

    // Check if the request was successful
    if (statusCode != 200)
    {
        error.devMessage = "Request failed with status code: " + juce::String(statusCode);
        return OpResult::fail(error);
    }

    // Remove file at target path if one already exists
    // Before adding this, the new file did not replace the old file
    // TODO - make file path unique (timestamp or Uuid)
    downloadedFile.deleteFile();

    // Create output stream to save the file locally
    std::unique_ptr<juce::FileOutputStream> fileOutput(downloadedFile.createOutputStream());

    if (fileOutput == nullptr || ! fileOutput->openedOk())
    {
        error.devMessage =
            "Failed to create output stream for file: " + downloadedFile.getFullPathName();
        return OpResult::fail(error);
    }

    // Copy data from the input stream to the output stream
    fileOutput->writeFromInputStream(*stream, stream->getTotalLength());

    // Store the file path where the file was downloaded
    downloadedFilePath = downloadedFile.getFullPathName();

    return OpResult::ok();
}

juce::String GradioClient::getAuthorizationHeader() const
{
    if (tokenEnabled && ! token.isEmpty())
    {
        return "Authorization: Bearer " + token + "\r\n";
    }
    return "";
}

juce::String GradioClient::getJsonContentTypeHeader() const
{
    return "Content-Type: application/json\r\n";
}

juce::String GradioClient::getAcceptHeader() const { return "Accept: */*\r\n"; }

juce::String GradioClient::createCommonHeaders() const
{
    return getAcceptHeader() + getAuthorizationHeader();
}

juce::String GradioClient::createJsonHeaders() const
{
    return getJsonContentTypeHeader() + getAcceptHeader() + getAuthorizationHeader();
}

OpResult GradioClient::validateToken(const juce::String& inputToken) const
{
    // Create the error here, in case we need it
    Error error;
    int statusCode = 0;

    juce::URL url = juce::URL("https://huggingface.co/api/whoami-v2");

    // Create a GET request to whoami-v2 API with provided token 
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders("Authorization: Bearer " + inputToken + "\r\n")
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

void GradioClient::setToken(const juce::String& inputToken) { this->token = inputToken; }

juce::String GradioClient::getToken() const { return token; }

void GradioClient::setTokenEnabled(bool enabled) { tokenEnabled = enabled; }