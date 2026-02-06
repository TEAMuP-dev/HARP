/**
 * @file StabilityClient.h
 * @brief Client specifics for Stability AI (multipart requests).
 * @author xribene, huiranyu, lindseydeng, cwitkowitz
 */

#pragma once

#include <BinaryData.h>

#include "../../Client.h"

using namespace juce;

struct MultipartRequest
{
    String boundary;
    MemoryOutputStream body;

    MultipartRequest() : boundary("----" + Uuid().toString().removeCharacters("-")) {}

    void addField(const String& name, const String& value)
    {
        if (value.isEmpty())
        {
            return;
        }

        body << "--" << boundary << "\r\n";
        body << "Content-Disposition: form-data; name=\"" << name << "\"\r\n\r\n";
        body << value << "\r\n";
    }

    OpResult addFile(const String& name, const File& file, const String& mimeType)
    {
        std::unique_ptr<FileInputStream> in(file.createInputStream());

        if (! in || ! in->openedOk())
        {
            return OpResult::fail(
                FileError { FileError::Type::UploadFailed, file.getFullPathName() });
        }

        body << "--" << boundary << "\r\n";
        body << "Content-Disposition: form-data; name=\"" << name << "\"; filename=\""
             << file.getFileName() << "\"\r\n";
        body << "Content-Type: " << mimeType << "\r\n\r\n";

        body.writeFromInputStream(*in, -1);
        body << "\r\n";

        return OpResult::ok();
    }

    void finish() { body << "--" << boundary << "--\r\n"; }

    String contentTypeHeader() const
    {
        return "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
    }
};

class StabilityClient : public Client
{
public:
    StabilityClient()
    {
        provider = Provider::Stability;

        acceptHeader = "Accept: audio/*,application/json\r\n";

        tokenValidationURL = URL("https://api.stability.ai/v1/user/account");
        tokenRegistrationURL = URL("https://platform.stability.ai/account/keys");
    }

    static bool matchesPathSpec(String modelPath)
    {
        return isValidTextToAudioPath(modelPath) || isValidAudioToAudioPath(modelPath);
    }

    String inferHostSlashModel(String modelPath) override
    {
        String hostSlashModel;

        if (isValidTextToAudioPath(modelPath))
        {
            hostSlashModel = "stability/text-to-audio";
        }
        else if (isValidAudioToAudioPath(modelPath))
        {
            hostSlashModel = "stability/audio-to-audio";
        }
        else
        {
            DBG_AND_LOG("StabilityClient::inferHostSlashModel: Path \""
                        << modelPath << "\" does not match valid specification for Stability AI.");
        }

        return hostSlashModel;
    }

    String inferEndpointPath(String modelPath) override
    {
        String endpointPath;

        if (isValidTextToAudioPath(modelPath))
        {
            endpointPath = "https://api.stability.ai/v2beta/audio/stable-audio-2/text-to-audio";
        }
        else if (isValidAudioToAudioPath(modelPath))
        {
            endpointPath = "https://api.stability.ai/v2beta/audio/stable-audio-2/audio-to-audio";
        }
        else
        {
            DBG_AND_LOG("StabilityClient::inferEndpointURL: Path \""
                        << modelPath << "\" does not match valid specification for Stability AI.");
        }

        return endpointPath;
    }

    String inferDocumentationPath(String modelPath) override
    {
        String documentationPath;

        if (isValidTextToAudioPath(modelPath))
        {
            documentationPath =
                "https://platform.stability.ai/docs/api-reference#tag/Stable-Audio-2/"
                "paths/~1v2beta~1audio~1stable-audio-2~1text-to-audio/post";
        }
        else if (isValidAudioToAudioPath(modelPath))
        {
            documentationPath =
                "https://platform.stability.ai/docs/api-reference#tag/Stable-Audio-2/"
                "paths/~1v2beta~1audio~1stable-audio-2~1audio-to-audio/post";
        }
        else
        {
            DBG_AND_LOG("StabilityClient::inferDocumentationURL: Path \""
                        << modelPath << "\" does not match valid specification for Stability AI.");
        }

        return documentationPath;
    }

    OpResult queryControls(String modelPath, DynamicObject::Ptr& controls)
    {
        const char* jsonData;
        int jsonDataSize = 0;

        if (isValidTextToAudioPath(modelPath))
        {
            DBG_AND_LOG("StabilityClient::queryControls: Reading text-to-audio.json.");

            // Access binarized JSON for text-to-audio controls
            jsonData = BinaryData::texttoaudio_json;
            jsonDataSize = BinaryData::texttoaudio_jsonSize;
        }
        else if (isValidAudioToAudioPath(modelPath))
        {
            DBG_AND_LOG("StabilityClient::queryControls: Reading audio-to-audio.json.");

            // Access binarized JSON for audio-to-audio controls
            jsonData = BinaryData::audiotoaudio_json;
            jsonDataSize = BinaryData::audiotoaudio_jsonSize;
        }
        else
        {
            ClientError error { ClientError::Type::InvalidModelPath, modelPath, "Stability AI" };

            DBG_AND_LOG("StabilityClient::queryControls: " << toUserMessage(error));

            return OpResult::fail(error);
        }

        String queryResponse = String::fromUTF8(jsonData, jsonDataSize);

        if (queryResponse.isEmpty())
        {
            return OpResult::fail(JsonError { JsonError::Type::Empty, queryResponse });
        }

        // Extract model metadata, inputs, controls, and outputs
        OpResult result = stringJSONToDict(queryResponse, controls);

        if (result.failed())
        {
            return result;
        }

        return OpResult::ok();
    }

    var wrapPayloadElement(var payloadElement, bool isFile = false, String label = "") override
    {
        DynamicObject::Ptr wrappedPayloadElement = new DynamicObject();

        String controlLabel = label.isNotEmpty() ? label : "control";

        // Use "input" label for audio file — required for Stability AI
        const String payloadLabel = isFile ? "input" : controlLabel;

        wrappedPayloadElement->setProperty("label", payloadLabel);
        wrappedPayloadElement->setProperty("value", payloadElement);

        return var(wrappedPayloadElement);
    }

    OpResult process(String modelPath,
                     String& payloadJSON,
                     std::vector<File>& outputFiles,
                     LabelList& labels)
    {
        DynamicObject::Ptr dataDict;

        OpResult result = stringJSONToDict(payloadJSON, dataDict);

        if (result.failed())
        {
            return result;
        }

        static const Identifier outputsKey { "data" };

        Array<var>* modelInputs;

        result = getRequiredArrayProperty(dataDict, outputsKey, modelInputs);

        if (result.failed())
        {
            return result;
        }

        auto getLabel = [&](int i) { return modelInputs->getReference(i)["label"].toString(); };
        auto getValue = [&](int i) { return modelInputs->getReference(i)["value"].toString(); };

        MultipartRequest request;

        int offsetIdx = 0;

        if (getLabel(0) == "input")
        {
            const File inputFile(modelInputs->getReference(0)["value"]["path"].toString());

            if (! inputFile.existsAsFile())
            {
                return OpResult::fail(
                    FileError { FileError::Type::DoesNotExist, inputFile.getFullPathName() });
            }

            const String mime = getMimeForAudioFile(inputFile);

            if (mime.isEmpty())
            {
                return OpResult::fail(
                    FileError { FileError::Type::UnsupportedFormat, inputFile.getFullPathName() });
            }

            result = request.addFile("audio", inputFile, mime);

            if (result.failed())
            {
                return result;
            }

            offsetIdx += 1;
        }

        request.addField("duration", getValue(offsetIdx + 0));
        request.addField("steps", getValue(offsetIdx + 1));
        request.addField("cfg_scale", getValue(offsetIdx + 2));
        request.addField("output_format", getValue(offsetIdx + 3));
        request.addField("prompt", getValue(offsetIdx + 4));

        request.finish();

        const String extension =
            getValue(offsetIdx + 3).isNotEmpty() ? "." + getValue(offsetIdx + 3) : ".wav";

        // Obtain local temporary directory for downloaded file
        File tempDir = File::getSpecialLocation(File::tempDirectory);

        // Create file at a unique path
        File outputFile = tempDir.getChildFile("stable-audio_" + Uuid().toString() + extension);

        result = makePOSTRequest(URL(inferEndpointPath(modelPath)),
                                 request,
                                 outputFile,
                                 inferDocumentationPath(modelPath));

        if (result.failed())
        {
            return result;
        }

        outputFiles.push_back(outputFile);

        ignoreUnused(labels);

        return OpResult::ok();
    }

private:
    static bool isValidTextToAudioPath(String modelPath)
    {
        return isValidShortTextToAudioPath(modelPath) || isValidLongTextToAudioPath(modelPath);
    }

    static bool isValidShortTextToAudioPath(String modelPath)
    {
        /*
          i.e., "stability/text-to-audio"
        */

        StringArray array = StringArray::fromTokens(modelPath, "/", "");

        return array.size() == 2 && array[0].equalsIgnoreCase("stability")
               && array[1].equalsIgnoreCase("text-to-audio");
    }

    static bool isValidLongTextToAudioPath(String modelPath)
    {
        /*
          i.e., "https://api.stability.ai/v2beta/audio/stable-audio-2/text-to-audio"
        */

        return modelPath
            .fromFirstOccurrenceOf(
                "https://api.stability.ai/v2beta/audio/stable-audio-2/", false, false)
            .equalsIgnoreCase("text-to-audio");
    }

    static bool isValidAudioToAudioPath(String modelPath)
    {
        return isValidShortAudioToAudioPath(modelPath) || isValidLongAudioToAudioPath(modelPath);
    }

    static bool isValidShortAudioToAudioPath(String modelPath)
    {
        /*
          i.e., "stability/audio-to-audio"
        */

        StringArray array = StringArray::fromTokens(modelPath, "/", "");

        return array.size() == 2 && array[0].equalsIgnoreCase("stability")
               && array[1].equalsIgnoreCase("audio-to-audio");
    }

    static bool isValidLongAudioToAudioPath(String modelPath)
    {
        /*
          i.e., "https://api.stability.ai/v2beta/audio/stable-audio-2/audio-to-audio"
        */

        return modelPath
            .fromFirstOccurrenceOf(
                "https://api.stability.ai/v2beta/audio/stable-audio-2/", false, false)
            .equalsIgnoreCase("audio-to-audio");
    }

    OpResult makePOSTRequest(URL endpoint,
                             const MultipartRequest& request,
                             File& fileToDownload,
                             const String errorPath = "",
                             const int timeoutMs = 10000)
    {
        String headers = request.contentTypeHeader() + getCommonHeaders();
        MemoryBlock body = request.body.getMemoryBlock();

        DBG_AND_LOG(
            "StabilityClient::makePOSTRequest: Attempting to make POST request for endpoint \""
            + endpoint.toString(true) + "\" with headers \""
            + toPrintableHeaders(headers)
            //+ "\" and body \"" + body.toString()
            + "\".");

        endpoint = endpoint.withPOSTData(body);

        if (! endpoint.isWellFormed())
        {
            return OpResult::fail(
                HttpError { HttpError::Type::InvalidURL, HttpError::Request::POST, errorPath });
        }

        int statusCode = 0;
        StringPairArray responseHeaders;

        auto options = URL::InputStreamOptions(URL::ParameterHandling::inPostData)
                           .withExtraHeaders(headers)
                           .withConnectionTimeoutMs(timeoutMs)
                           .withResponseHeaders(&responseHeaders)
                           .withStatusCode(&statusCode)
                           .withNumRedirectsToFollow(5)
                           .withHttpRequestCmd("POST");

        std::unique_ptr<InputStream> stream(endpoint.createInputStream(options));

        if (stream == nullptr)
        {
            return OpResult::fail(HttpError {
                HttpError::Type::ConnectionFailed, HttpError::Request::POST, errorPath });
        }

        DBG_AND_LOG("StabilityClient::makePOSTRequest: Received status code \""
                    << String(statusCode) << "\" and response \""
                    << toPrintableHeaders(responseHeaders.getDescription()) << "\".");

        if (statusCode != 200)
        {
            String response = stream->readEntireStreamAsString();

            if (response.contains("authorization"))
            {
                return OpResult::fail(
                    ClientError { ClientError::Type::InsufficientPermissions, "", "Stability AI" });
            }
            // TODO - could potentially identify other errors (e.g., copyrighted material)
            else
            {
                return OpResult::fail(HttpError { HttpError::Type::BadStatusCode,
                                                  HttpError::Request::POST,
                                                  errorPath,
                                                  statusCode });
            }
        }

        // Remove file at target path if one already exists
        fileToDownload.deleteFile();

        // Create output stream to save file locally
        std::unique_ptr<FileOutputStream> outputFileStream(fileToDownload.createOutputStream());

        if (! outputFileStream || ! outputFileStream->openedOk())
        {
            return OpResult::fail(
                FileError { FileError::Type::DownloadFailed, endpoint.toString(true) });
        }

        // Copy data from POST request stream to stream for output file
        outputFileStream->writeFromInputStream(*stream, stream->getTotalLength());

        return OpResult::ok();
    }

    String getMimeForAudioFile(const File& f)
    {
        static const std::unordered_map<String, String> mimeLookup { { ".wav", "audio/wav" },
                                                                     { ".wave", "audio/wav" },
                                                                     { ".mp3", "audio/mpeg" } };

        String mime;

        auto it = mimeLookup.find(f.getFileExtension().toLowerCase());

        if (it != mimeLookup.end())
        {
            mime = it->second;
        }

        return mime;
    }
};
