#include "GradioClient.h"
#include "../errors.h"
#include "../external/magic_enum.hpp"

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

OpResult GradioClient::parseSpaceAddress(juce::String spaceAddress, SpaceInfo& spaceInfo)
{
    // Create an Error object in case we need it
    Error error;
    // All errors in this function are of type InvalidURL
    error.type = ErrorType::InvalidURL;

    spaceInfo.userInput = spaceAddress;
    // Check if the URL is of Type 4 (localhost or gradio.live)
    if (spaceAddress.contains("localhost") || spaceAddress.contains("gradio.live")
        || spaceAddress.matchesWildcard("*.*.*.*:*", true))
    {
        spaceInfo.gradio = spaceAddress;
        spaceInfo.huggingface = spaceAddress;
        spaceInfo.status = SpaceInfo::Status::LOCALHOST;
        return OpResult::ok();
    }
    juce::String user;
    juce::String model;

    juce::String huggingFaceBaseUrl = "https://huggingface.co/spaces/";
    if (spaceAddress.contains(huggingFaceBaseUrl))
    {
        // Remove the base URL part
        auto spacePath = spaceAddress.fromFirstOccurrenceOf(huggingFaceBaseUrl, false, false);

        // Split the path into user and model using '/'
        auto parts = juce::StringArray::fromTokens(spacePath, "/", "");

        if (parts.size() >= 2)
        {
            user = parts[0];
            model = parts[1];
            spaceInfo.status = SpaceInfo::Status::HUGGINGFACE;
        }
        else
        {
            // result.huggingface = spaceAddress;
            spaceInfo.error = "Detected huggingface.co URL but could not parse user and "
                              "model. Too few parts in "
                              + spaceAddress;
            spaceInfo.status = SpaceInfo::Status::ERROR;
            error.devMessage = spaceInfo.error;
            return OpResult::fail(error);
        }
    }
    else if (spaceAddress.contains("hf.space"))
    {
        // Remove the protocol part (e.g., "https://")
        auto withoutProtocol = spaceAddress.fromFirstOccurrenceOf("://", false, false);

        // Extract the subdomain part before ".hf.space/"
        auto subdomain = withoutProtocol.upToFirstOccurrenceOf(".hf.space", false, false);

        // Split the subdomain at the first hyphen
        auto firstHyphenIndex = subdomain.indexOfChar('-');
        if (firstHyphenIndex != -1)
        {
            user = subdomain.substring(0, firstHyphenIndex);
            model = subdomain.substring(firstHyphenIndex + 1);
            spaceInfo.status = SpaceInfo::Status::GRADIO;
        }
        else
        {
            // DBG("No hyphen found in the subdomain." << subdomain);
            // Even though the spaceAddress is supposed to be a gradio URL, we
            // return it as the huggingface URL because result.huggingface is
            // used for the  "Open Space URL" button in the error dialog box
            // result.huggingface = spaceAddress;
            spaceInfo.error = "Detected hf.space URL but could not parse user and model. No "
                              "hyphen found in the subdomain: "
                              + subdomain;
            spaceInfo.status = SpaceInfo::Status::ERROR;
            error.devMessage = spaceInfo.error;
            return OpResult::fail(error);
        }
    }
    // else if address is of the form user/model and doesn't contain http
    else if (spaceAddress.contains("/") && ! spaceAddress.contains("http"))
    {
        // Extract user and model
        auto parts = juce::StringArray::fromTokens(spaceAddress, "/", "");
        if (parts.size() == 2)
        {
            user = parts[0];
            model = parts[1];
            spaceInfo.status = SpaceInfo::Status::HUGGINGFACE;
        }
        else
        {
            // DBG("Invalid URL: " << spaceAddress);
            // result.huggingface = spaceAddress;
            spaceInfo.error = "Detected user/model URL but could not parse user and model. "
                              "Too many/few slashes in "
                              + spaceAddress;
            spaceInfo.status = SpaceInfo::Status::ERROR;
            error.devMessage = spaceInfo.error;
            return OpResult::fail(error);
        }
    }
    else
    {
        spaceInfo.error =
            "Invalid URL: " + spaceAddress + ". URL does not match any of the expected patterns.";
        spaceInfo.status = SpaceInfo::Status::ERROR;
        error.devMessage = spaceInfo.error;
        return OpResult::fail(error);
    }

    // Construct the gradio and HF URLs
    if (! user.isEmpty() && ! model.isEmpty())
    {
        // model = model.replace("-", "_");
        if (spaceInfo.status == SpaceInfo::Status::HUGGINGFACE)
        {
            // spaceInfo.huggingface = spaceAddress;
            spaceInfo.huggingface = "https://huggingface.co/spaces/" + user + "/" + model;
            // the embedded gradio app URL doesn't contain "_". So if the model name
            // contains "_", we replace it with "-"
            // model = model.replace("_", "-");
            spaceInfo.gradio = "https://" + user + "-" + model.replace("_", "-") + ".hf.space";
        }
        else if (spaceInfo.status == SpaceInfo::Status::GRADIO)
        {
            // If the user provided spaceAddress is an embedded gradio app URL,
            // there is no way to get the huggingface URL from it, since we don't
            // know if the "-" in the URL is a hyphen or an underscore that was
            // replaced by a hyphen.
            spaceInfo.gradio = spaceAddress;
            spaceInfo.huggingface = "";
        }
        spaceInfo.userName = user;
        // model name might have "-" instead of "_" if the user provided the embedded gradio app URL
        spaceInfo.modelName = model;
    }
    else
    {
        spaceInfo.error = "Unkown error while parsing the space address: " + spaceAddress;
        spaceInfo.status = SpaceInfo::Status::ERROR;
        error.devMessage = spaceInfo.error;
        return OpResult::fail(error);
    }
    LogAndDBG(spaceInfo.toString());
    return OpResult::ok();
}

OpResult GradioClient::setSpaceInfo(const juce::String userProvidedSpaceAddress)
{
    return parseSpaceAddress(userProvidedSpaceAddress, spaceInfo);
}

SpaceInfo GradioClient::getSpaceInfo() const { return spaceInfo; }

OpResult GradioClient::uploadFileRequest(const juce::File& fileToUpload,
                                         juce::String& uploadedFilePath,
                                         const int timeoutMs) const
{
    juce::URL gradioEndpoint = spaceInfo.gradio;
    juce::URL uploadEndpoint = gradioEndpoint.getChildURL("upload");

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
                       // .withExtraHeaders("Accept: */*")
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
    juce::URL requestEndpoint = gradioEndpoint.getChildURL("call").getChildURL(endpoint);

    // Prepare the POST request
    // juce::String jsonBody = R"({"data": []})";
    juce::URL postEndpoint = requestEndpoint.withPOSTData(jsonBody);
    juce::StringPairArray responseHeaders;
    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                       .withExtraHeaders("Content-Type: application/json\r\nAccept: */*")
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
    juce::URL getEndpoint =
        gradioEndpoint.getChildURL("call").getChildURL(callID).getChildURL(eventID);
    juce::StringPairArray responseHeaders;
    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       //    .withExtraHeaders("Content-Type: application/json\r\nAccept:
                       //    */*")
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
    while (!stream->isExhausted())
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

OpResult GradioClient::getControls(juce::Array<juce::var>& ctrlList, juce::DynamicObject& cardDict)
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

    juce::Array<juce::var>* ctrlArray = obj->getProperty("ctrls").getArray();
    if (ctrlArray == nullptr)
    {
        error.devMessage = "Couldn't load the controls array/list from the controls response.";
        return OpResult::fail(error);
    }
    ctrlList = *ctrlArray;

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
    juce::File downloadedFile = tempDir.getChildFile(fileName);

    // Create input stream to download the file
    juce::StringPairArray responseHeaders;
    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
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
