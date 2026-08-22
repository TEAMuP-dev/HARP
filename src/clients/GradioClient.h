/**
 * @file GradioClient.h
 * @brief Client specifics for Gradio and Hugging Face (simple JSON requests).
 * @author xribene, huiranyu, cwitkowitz, saumya-pailwan
 */

#pragma once

#include <map>

#include "Client.h"

using namespace juce;

class GradioClient : public Client
{
public:
    enum GradioEvents
    {
        Complete,
        Heartbeat,
        Error
    };

    GradioClient()
    {
        provider = Provider::HuggingFace;

        acceptHeader = "Accept: */*\r\n";
        contentTypeJSONHeader = "Content-Type: application/json\r\n";

        tokenValidationURL = URL("https://huggingface.co/api/whoami-v2");
        tokenRegistrationURL = URL("https://huggingface.co/settings/tokens");
    }

    static bool matchesPathSpec(String modelPath)
    {
        return isValidLocalPath(modelPath) || isValidGradioPath(modelPath)
               || isValidHuggingFacePath(modelPath);
    }

    String inferHostSlashModel(String modelPath) override
    {
        String hostSlashModel;

        if (isValidLocalPath(modelPath) || isValidGradioPath(modelPath))
        {
            hostSlashModel = "localhost";
        }
        else if (isValidHuggingFacePath(modelPath))
        {
            if (isValidShortHuggingFacePath(modelPath))
            {
                StringArray array = StringArray::fromTokens(
                    modelPath.fromFirstOccurrenceOf("https://", false, false)
                        .upToFirstOccurrenceOf(".hf.space", false, false),
                    "-",
                    "");

                hostSlashModel = array[0] + "/" + array[1];
            }
            else if (isValidLongHuggingFacePath(modelPath))
            {
                StringArray array = StringArray::fromTokens(
                    modelPath.fromFirstOccurrenceOf("https://huggingface.co/spaces/", false, false),
                    "/",
                    "");

                hostSlashModel = array[0] + "/" + array[1];
            }
            else if (isValidAbbrevHuggingFacePath(modelPath))
            {
                hostSlashModel = modelPath;
            }
        }
        else
        {
            DBG_AND_LOG("GradioClient::inferHostSlashModel: Path \""
                        << modelPath << "\" does not match valid specification for Gradio.");
        }

        return hostSlashModel;
    }

    String inferEndpointPath(String modelPath) override
    {
        String endpointPath;

        if (isValidLocalPath(modelPath))
        {
            endpointPath = modelPath;

            if (! endpointPath.startsWith("http://"))
            {
                endpointPath = "http://" + endpointPath;
            }
        }
        else if (isValidGradioPath(modelPath))
        {
            endpointPath = modelPath;
        }
        else if (isValidHuggingFacePath(modelPath))
        {
            if (isValidShortHuggingFacePath(modelPath))
            {
                endpointPath = modelPath;
            }
            else if (isValidLongHuggingFacePath(modelPath)
                     || isValidAbbrevHuggingFacePath(modelPath))
            {
                String hostSlashModel = inferHostSlashModel(modelPath);

                auto array = StringArray::fromTokens(hostSlashModel, "/", "");

                String host = array[0];
                String model = array[1];

                // TODO - this can load paths that were incorrectly added with "-" instead of "_" resulting in a broken documentation link
                endpointPath = "https://" + host + "-" + model.replace("_", "-") + ".hf.space/";
            }
        }
        else
        {
            DBG_AND_LOG("GradioClient::inferEndpointURL: Path \""
                        << modelPath << "\" does not match valid specification for Gradio.");
        }

        return endpointPath;
    }

    String inferDocumentationPath(String modelPath) override
    {
        String documentationPath;

        if (isValidLocalPath(modelPath) || isValidGradioPath(modelPath))
        {
            documentationPath = inferEndpointPath(modelPath);
        }
        else if (isValidHuggingFacePath(modelPath))
        {
            if (isValidShortHuggingFacePath(modelPath) || isValidAbbrevHuggingFacePath(modelPath))
            {
                documentationPath =
                    "https://huggingface.co/spaces/" + inferHostSlashModel(modelPath);
            }
            else if (isValidLongHuggingFacePath(modelPath))
            {
                documentationPath = modelPath;
            }
        }
        else
        {
            DBG_AND_LOG("GradioClient::inferDocumentationPath: Path \""
                        << modelPath << "\" does not match valid specification for Gradio.");
        }

        return documentationPath;
    }

    virtual OpResult validateToken(const String& tokenToValidate) override
    {
        String responseJSON;

        OpResult result = queryToken(tokenToValidate, responseJSON);

        if (result.failed())
        {
            return result;
        }

        DynamicObject::Ptr responseDict;

        result = stringJSONToDict(responseJSON, responseDict);

        if (result.failed())
        {
            return result;
        }

        auto* tokenJSON = responseDict->getProperty("auth")
                              .getDynamicObject()
                              ->getProperty("accessToken")
                              .getDynamicObject();

        String role = tokenJSON->getProperty("role").toString();

        if (! (role == "write" || role == "read"))
        {
            bool hasAllPermissions = false;

            auto* scopedArray = tokenJSON->getProperty("fineGrained")
                                    .getDynamicObject()
                                    ->getProperty("scoped")
                                    .getArray();

            for (const auto& scopeEntry : *scopedArray)
            {
                if (! scopeEntry.isObject())
                    continue;

                var permissionsVar = scopeEntry.getDynamicObject()->getProperty("permissions");

                if (! permissionsVar.isArray())
                    continue;

                auto* permissionsArray = permissionsVar.getArray();
                bool hasAll = permissionsArray->contains("repo.content.read")
                              && permissionsArray->contains("repo.write")
                              && permissionsArray->contains("inference.serverless.write")
                              && permissionsArray->contains("inference.endpoints.infer.write");

                if (hasAll)
                {
                    hasAllPermissions = true;
                    break;
                }
            }

            if (! hasAllPermissions)
            {
                return OpResult::fail(ClientError { ClientError::Type::InsufficientPermissions,
                                                    "",
                                                    "Hugging Face",
                                                    tokenToValidate });
            }
        }

        return OpResult::ok();
    }

    OpResult queryControls(String modelPath, DynamicObject::Ptr& controls)
    {
        String responseJSON;

        OpResult result = makeRequest(modelPath, "controls", emptyJSONBody, responseJSON);

        if (result.failed())
        {
            return result;
        }

        Array<var> dataList;

        result = stringJSONToList(responseJSON, dataList);

        if (result.failed())
        {
            return result;
        }

        if (dataList.isEmpty())
        {
            return OpResult::fail(JsonError { JsonError::Type::Empty, {} });
        }

        var first = dataList.getFirst();

        if (! first.isObject())
        {
            return OpResult::fail(JsonError { JsonError::Type::NotADictionary, first.toString() });
        }

        // Extract model metadata, inputs, controls, and outputs
        controls = first.getDynamicObject();

        if (controls == nullptr)
        {
            return OpResult::fail(JsonError { JsonError::Type::InvalidJSON, first.toString() });
        }

        return OpResult::ok();
    }

    OpResult uploadFile(String modelPath, const File& fileToUpload, String& remoteFilePath) override
    {
        URL endpoint =
            URL(inferEndpointPath(modelPath)).getChildURL("gradio_api").getChildURL("upload");

        String requestBody; // Empty
        String responseJSON;

        OpResult result = makePOSTRequest(endpoint,
                                          getCommonHeaders(),
                                          requestBody,
                                          responseJSON,
                                          inferDocumentationPath(modelPath),
                                          fileToUpload,
                                          10000,
                                          modelPath);

        if (result.failed())
        {
            return result;
        }

        Array<var> responseArray;

        result = stringJSONToList(responseJSON, responseArray);

        if (result.failed())
        {
            return result;
        }

        remoteFilePath = responseArray.getFirst().toString();

        if (remoteFilePath.isEmpty())
        {
            return OpResult::fail(
                FileError { FileError::Type::UploadFailed, fileToUpload.getFullPathName() });
        }

        return OpResult::ok();
    }

    var wrapPayloadElement(var payloadElement, bool isFile = false, String label = "") override
    {
        ignoreUnused(label);

        if (isFile and ! payloadElement.isVoid())
        {
            DynamicObject::Ptr wrappedPayloadElement = payloadElement.getDynamicObject();

            DynamicObject::Ptr meta = new DynamicObject();

            meta->setProperty("_type", var("gradio.FileData"));
            wrappedPayloadElement->setProperty("meta", var(meta));

            return var(wrappedPayloadElement);
        }
        else
        {
            return payloadElement;
        }
    }

    OpResult process(String modelPath,
                     String& payloadJSON,
                     std::vector<File>& outputFiles,
                     LabelList& labels)
    {
        String responseJSON;

        OpResult result =
            makeRequest(modelPath, "process", payloadJSON, responseJSON, processTimeoutMs);

        if (result.failed())
        {
            return result;
        }

        Array<var> dataList;

        result = stringJSONToList(responseJSON, dataList);

        if (result.failed())
        {
            return result;
        }

        if (dataList.isEmpty())
        {
            return OpResult::fail(JsonError { JsonError::Type::Empty, {} });
        }

        for (int i = 0; i < dataList.size(); i++)
        {
            var outputVar = dataList.getReference(i);

            if (! outputVar.isObject())
            {
                return OpResult::fail(
                    JsonError { JsonError::Type::NotADictionary, outputVar.toString() });
            }

            DynamicObject::Ptr outputDict = outputVar.getDynamicObject();

            static const Identifier metadataKey { "meta" };

            if (outputDict->hasProperty(metadataKey))
            {
                var metadata = outputDict->getProperty(metadataKey);

                if (! metadata.isObject())
                {
                    return OpResult::fail(
                        JsonError { JsonError::Type::NotADictionary, metadata.toString() });
                }

                static const Identifier typeKey { "_type" };

                String outputType = metadata.getDynamicObject()->getProperty(typeKey).toString();

                if (outputType == "gradio.FileData")
                {
                    File outputFile;

                    String remotePath = outputDict->getProperty("url").toString();

                    result = downloadFile(remotePath, outputFile);

                    if (result.failed())
                    {
                        return result;
                    }

                    outputFiles.push_back(outputFile);
                }
                else if (outputType == "pyharp.LabelList")
                {
                    result = extractLabels(outputDict, labels);

                    if (result.failed())
                    {
                        return result;
                    }
                }
                else
                {
                    // Unknown output type
                    jassertfalse;
                }
            }
            else
            {
                return OpResult::fail(JsonError { JsonError::Type::MissingKey,
                                                  JSON::toString(var(outputDict), true),
                                                  metadataKey.toString() });
            }
        }

        return OpResult::ok();
    }

    OpResult cancel(String modelPath)
    {
        String response;

        OpResult result = makeRequest(modelPath, "cancel", emptyJSONBody, response);

        if (result.failed())
        {
            return result;
        }

        return OpResult::ok();
    }

private:
    static bool isValidLocalPath(String modelPath)
    {
        /*
          e.g., "http://localhost:7860" or "http://127.0.0.1:7860"
        */

        return modelPath.contains("localhost") || modelPath.matchesWildcard("*.*.*.*:*", true);
    }

    static bool isValidGradioPath(String modelPath)
    {
        /*
          e.g., "https://<RANDOM_STRING>.gradio.live"
        */

        return modelPath.startsWith("https://") && modelPath.endsWith(".gradio.live");
    }

    static bool isValidHuggingFacePath(String modelPath)
    {
        return isValidShortHuggingFacePath(modelPath) || isValidLongHuggingFacePath(modelPath)
               || isValidAbbrevHuggingFacePath(modelPath);
    }

    static bool isValidShortHuggingFacePath(String modelPath)
    {
        /*
          e.g., "https://xribene-midi-pitch-shifter.hf.space/"
        */

        StringArray array =
            StringArray::fromTokens(modelPath.fromFirstOccurrenceOf("https://", false, false)
                                        .upToFirstOccurrenceOf(".hf.space", false, false),
                                    "-",
                                    "");

        if (array.size() == 2)
        {
            return true;
        }
        else if (array.size() != 0)
        {
            /*
              This is ambiguous. There's no way to tell where the delimeter
              belongs and which hypens were converted from underscores.
            */

            DBG_AND_LOG(
                "GradioClient::isValidShortHuggingFacePath: Path \""
                << modelPath
                << "\" is ambiguous. Please use the abbreviated or long-form path instead.");

            return false;
        }
        else
        {
            return false;
        }
    }

    static bool isValidLongHuggingFacePath(String modelPath)
    {
        /*
          e.g., "https://huggingface.co/spaces/xribene/midi_pitch_shifter"
        */

        return isValidAbbrevHuggingFacePath(
            modelPath.fromFirstOccurrenceOf("https://huggingface.co/spaces/", false, false));
    }

    static bool isValidAbbrevHuggingFacePath(String modelPath)
    {
        /*
          e.g., "xribene/midi_pitch_shifter"
        */

        StringArray array = StringArray::fromTokens(modelPath, "/", "");

        return array.size() == 2;
    }

    /* The lifecycle stage reported by the Hub, e.g. RUNNING, SLEEPING, APP_STARTING,
       BUILDING, RUNTIME_ERROR, PAUSED. Returns an empty string when it cannot be
       determined. Unlike the hardware type this changes constantly, so it is never
       cached. */
    String querySpaceStage(const String& modelPath)
    {
        if (! isValidHuggingFacePath(modelPath))
            return {};

        URL runtimeEndpoint =
            URL("https://huggingface.co/api/spaces/" + inferHostSlashModel(modelPath) + "/runtime");

        std::unique_ptr<InputStream> stream;

        if (makeGETRequestStream(runtimeEndpoint, stream, "", 5000).failed() || stream == nullptr)
            return {};

        DynamicObject::Ptr responseDict;

        if (stringJSONToDict(stream->readEntireStreamAsString(), responseDict).failed())
            return {};

        static const Identifier stageKey { "stage" };

        return responseDict->hasProperty(stageKey)
                   ? responseDict->getProperty(stageKey).toString().toUpperCase()
                   : String();
    }

    /* Turns a Space stage into the error it should be reported as. Asking the Hub
       directly avoids guessing from the status code alone: a Space that is merely
       waking up answers a request exactly like one that is genuinely broken. */
    static bool isStartingStage(const String& stage)
    {
        return stage == "SLEEPING" || stage == "APP_STARTING" || stage == "BUILDING"
               || stage == "RUNNING";
    }

    static bool isUnavailableStage(const String& stage)
    {
        return stage == "RUNTIME_ERROR" || stage == "BUILD_ERROR" || stage == "CONFIG_ERROR"
               || stage == "NO_APP_FILE" || stage == "PAUSED" || stage == "STOPPED"
               || stage == "DELETING";
    }

    bool isZeroGPUSpace(const String& modelPath)
    {
        if (! isValidHuggingFacePath(modelPath))
            return false;

        // The hardware type is a static property of the space, so cache it to
        // avoid repeated blocking lookups from error-handling paths
        {
            const ScopedLock lock(zeroGPUCacheLock);

            auto cached = zeroGPUCache.find(modelPath);

            if (cached != zeroGPUCache.end())
                return cached->second;
        }

        URL runtimeEndpoint =
            URL("https://huggingface.co/api/spaces/" + inferHostSlashModel(modelPath) + "/runtime");

        std::unique_ptr<InputStream> stream;

        // Failed lookups are not cached so a transient network error
        // does not permanently misclassify the space
        if (makeGETRequestStream(runtimeEndpoint, stream, "", 5000).failed() || stream == nullptr)
            return false;

        String responseJSON = stream->readEntireStreamAsString();

        DynamicObject::Ptr responseDict;

        if (stringJSONToDict(responseJSON, responseDict).failed())
            return false;

        // Navigate: hardware -> current
        static const Identifier hardwareKey { "hardware" };
        static const Identifier currentKey { "current" };

        String hardwareCurrent;

        if (responseDict->hasProperty(hardwareKey))
        {
            if (auto* hardwareDict = responseDict->getProperty(hardwareKey).getDynamicObject())
                hardwareCurrent = hardwareDict->getProperty(currentKey).toString();
        }

        // ZeroGPU hardware names start with "zero-" (e.g. "zero-a10g", "zero-a100")
        bool isZeroGPU = hardwareCurrent.startsWithIgnoreCase("zero-");

        {
            const ScopedLock lock(zeroGPUCacheLock);
            zeroGPUCache[modelPath] = isZeroGPU;
        }

        return isZeroGPU;
    }

    OpResult makePOSTRequest(URL endpoint,
                             const String headers,
                             const String body,
                             String& response,
                             const String errorPath = "",
                             const File& fileToUpload = File(),
                             const int timeoutMs = 10000,
                             const String modelPath = "")
    {
        String debugMessage =
            "GradioClient::makePOSTRequest: Attempting to make POST request for endpoint \""
            + endpoint.toString(true) + "\" with headers \"" + toPrintableHeaders(headers);

        if (body.isNotEmpty())
        {
            endpoint = endpoint.withPOSTData(body);

            debugMessage += "\" and body \"" + body;
        }

        if (fileToUpload.existsAsFile())
        {
            endpoint = endpoint.withFileToUpload("files", fileToUpload, "audio/midi");

            debugMessage += "\" and file \"" + fileToUpload.getFullPathName();
        }

        debugMessage += "\".";

        DBG_AND_LOG(debugMessage);

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

        DBG_AND_LOG("GradioClient::makePOSTRequest: Received status code \""
                    << String(statusCode) << "\" and response \""
                    << toPrintableHeaders(responseHeaders.getDescription()) << "\".");

        response = stream->readEntireStreamAsString();

        if (statusCode != 200)
        {
            String reason;

            // HTML bodies (proxy/login/error pages) carry no useful diagnostics
            if (! isHTMLResponse(response))
            {
                reason = extractShortReason(extractErrorInfoFromPayload(response).combined());
            }

            if (statusMessage != nullptr && reason.isNotEmpty())
            {
                statusMessage->setMessage("[error] " + reason);
            }

            /* A 503 from a Space is ambiguous on its own: the Hub answers the same way
               whether the Space is waking up or genuinely broken. Ask the Hub which it
               is rather than reporting it as down. */
            if (statusCode == 503 && modelPath.isNotEmpty())
            {
                String stage = querySpaceStage(modelPath);

                DBG_AND_LOG("GradioClient::makePOSTRequest: Space stage \"" << stage << "\".");

                if (isStartingStage(stage))
                {
                    return OpResult::fail(
                        GradioError { GradioError::Type::SpaceStarting, errorPath, stage });
                }

                if (isUnavailableStage(stage))
                {
                    return OpResult::fail(
                        GradioError { GradioError::Type::SpaceUnavailable, errorPath, stage });
                }
            }

            // Keep the real status code so callers can react to specific
            // failures (e.g. 404 for an invalid path, 402 for quota limits)
            // and attach any diagnostic text from the response body
            return OpResult::fail(HttpError { HttpError::Type::BadStatusCode,
                                              HttpError::Request::POST,
                                              errorPath,
                                              statusCode,
                                              reason });
        }

        if (isHTMLResponse(response))
        {
            // A 200 with an HTML body is not a Gradio API response; this typically
            // means an HF proxy page for a Space that is sleeping or broken
            return OpResult::fail(HttpError {
                HttpError::Type::BadStatusCode, HttpError::Request::POST, errorPath, 503 });
        }

        return OpResult::ok();
    }

    OpResult makeGETRequestStream(const URL endpoint,
                                  std::unique_ptr<InputStream>& stream,
                                  const String errorPath = "",
                                  const int timeoutMs = -1)
    {
        String requestHeaders = getCommonHeaders();

        DBG_AND_LOG(
            "GradioClient::makeGETRequestStream: Attempting to make GET request for endpoint \""
            << endpoint.toString(true) << "\" with headers \"" << toPrintableHeaders(requestHeaders)
            << "\".");

        if (! endpoint.isWellFormed())
        {
            return OpResult::fail(
                HttpError { HttpError::Type::InvalidURL, HttpError::Request::GET, errorPath });
        }

        int statusCode = 0;
        StringPairArray responseHeaders;

        auto options = URL::InputStreamOptions(URL::ParameterHandling::inAddress)
                           .withExtraHeaders(requestHeaders)
                           .withConnectionTimeoutMs(timeoutMs)
                           .withResponseHeaders(&responseHeaders)
                           .withStatusCode(&statusCode)
                           .withNumRedirectsToFollow(5);

        stream = endpoint.createInputStream(options);

        if (stream == nullptr)
        {
            return OpResult::fail(HttpError {
                HttpError::Type::ConnectionFailed, HttpError::Request::GET, errorPath });
        }

        DBG_AND_LOG("GradioClient::makeGETRequestStream: Received status code \""
                    << String(statusCode) << "\" and response \""
                    << toPrintableHeaders(responseHeaders.getDescription()) << "\".");

        if (statusCode != 200)
        {
            return OpResult::fail(HttpError {
                HttpError::Type::BadStatusCode, HttpError::Request::GET, errorPath, statusCode });
        }

        return OpResult::ok();
    }

    String extractPayload(String response)
    {
        String payload = response.trim();

        if (payload.startsWith("data:"))
        {
            payload = payload.fromFirstOccurrenceOf("data:", false, false).trim();
        }

        return payload;
    }

    // Compares a parsed SSE event type (the text after "event:") to a known event.
    // Matching on the parsed type rather than the raw line keeps this tolerant of
    // the SSE spec's optional space after the colon.
    static bool isEventType(const String& eventType, GradioEvents event)
    {
        return eventType.equalsIgnoreCase(enumToString(event));
    }

    /* What a Gradio error payload carries.

       Gradio >= 6.13 forwards the failing event's error payload on this endpoint:
         {"error": "<message>", "title": "<title>", "duration": .., "visible": ..}
       The message is populated whenever the server raised a gr.Error (which is how
       ZeroGPU reports quota exhaustion, with title "ZeroGPU quota exceeded"), and
       for any other exception only when the app was launched with show_error=True.
       When it was not, the payload is {"error": null} — a real runtime error whose
       details the server deliberately withheld.

       Gradio <= 6.12 discards the payload entirely and sends "data: null", so
       nothing at all can be told apart from the response. */
    struct GradioErrorInfo
    {
        String title;
        String message;

        // True when the server sent no payload at all, as opposed to a payload
        // that was present but carried no message
        bool payloadAbsent = false;

        String combined() const
        {
            if (title.isNotEmpty() && message.isNotEmpty())
                return title + ": " + message;

            return title.isNotEmpty() ? title : message;
        }
    };

    static GradioErrorInfo extractErrorInfoFromPayload(const String& payload)
    {
        GradioErrorInfo info;

        String normalizedPayload = payload.trim();

        if (normalizedPayload.isEmpty() || normalizedPayload.equalsIgnoreCase("null")
            || normalizedPayload.equalsIgnoreCase("none"))
        {
            info.payloadAbsent = true;

            return info;
        }

        var parsedPayload = JSON::parse(normalizedPayload);

        if (auto* payloadDict = parsedPayload.getDynamicObject())
        {
            info.title = extractFirstNonEmptyField(payloadDict, { "title" });
            info.message =
                extractFirstNonEmptyField(payloadDict, { "error", "message", "detail", "reason" });

            return info;
        }

        // A non-dictionary payload (e.g. a bare error string)
        info.message = normalizedPayload;

        return info;
    }

    static bool isHTMLResponse(const String& text)
    {
        String trimmed = text.trimStart();
        return trimmed.startsWithIgnoreCase("<!DOCTYPE") || trimmed.startsWithIgnoreCase("<html");
    }

    // Returns true when the HF proxy reports that the Space itself is broken
    static bool isSpaceStatusError(const String& text)
    {
        String lower = text.toLowerCase();
        return lower.contains("your space is in error")
               || lower.contains("space is in error")
               || lower.contains("check its status on hf");
    }

    static String extractShortReason(const String& text)
    {
        String firstLine = text.upToFirstOccurrenceOf("\n", false, false).trim();

        if (firstLine.length() > 200)
        {
            firstLine = firstLine.substring(0, 200).trimEnd() + "...";
        }

        return firstLine;
    }

    static String extractMessageTypeFromPayload(const String& payload)
    {
        String normalizedPayload = payload.trim();

        if (normalizedPayload.isEmpty() || normalizedPayload.equalsIgnoreCase("null")
            || normalizedPayload.equalsIgnoreCase("none"))
        {
            return "";
        }

        var parsedPayload = JSON::parse(normalizedPayload);

        if (auto* payloadDict = parsedPayload.getDynamicObject())
        {
            if (payloadDict->hasProperty("msg"))
            {
                String value = payloadDict->getProperty("msg").toString().trim();

                if (value.isNotEmpty() && ! value.equalsIgnoreCase("null"))
                {
                    return value;
                }
            }
        }

        return "";
    }

    static String extractFirstNonEmptyField(const DynamicObject* object,
                                            std::initializer_list<const char*> keys)
    {
        if (object == nullptr)
        {
            return "";
        }

        for (auto key : keys)
        {
            Identifier id(key);

            if (! object->hasProperty(id))
            {
                continue;
            }

            String value = object->getProperty(id).toString().trim();

            if (value.isNotEmpty() && ! value.equalsIgnoreCase("null")
                && ! value.equalsIgnoreCase("undefined"))
            {
                return value;
            }
        }

        return "";
    }

    static String extractGradioAlertStatusFromPayload(const String& payload)
    {
        String normalizedPayload = payload.trim();

        if (normalizedPayload.isEmpty() || normalizedPayload.equalsIgnoreCase("null")
            || normalizedPayload.equalsIgnoreCase("none"))
        {
            return "";
        }

        var parsedPayload = JSON::parse(normalizedPayload);
        auto* payloadDict = parsedPayload.getDynamicObject();

        if (payloadDict == nullptr)
        {
            return "";
        }

        String severity =
            extractFirstNonEmptyField(payloadDict, { "type", "level", "severity", "status" })
                .toLowerCase();
        String message = extractFirstNonEmptyField(
            payloadDict, { "message", "detail", "error", "reason", "log" });

        if (message.isEmpty())
        {
            var outputVar = payloadDict->getProperty("output");

            if (auto* outputDict = outputVar.getDynamicObject())
            {
                message = extractFirstNonEmptyField(outputDict,
                                                    { "message", "detail", "error", "reason" });
            }
        }

        if (message.isEmpty())
        {
            return "";
        }

        if (severity.isEmpty() || severity == "success")
        {
            if (message.containsIgnoreCase("error"))
            {
                severity = "error";
            }
            else
            {
                severity = "info";
            }
        }

        return "[" + severity + "] " + message;
    }

    // Formats a "log" SSE event payload (e.g. {"log": "...", "level": "info"})
    // into a user-visible status string like "[info] Starting inference...".
    // Returns empty string if the payload is not a valid log event.
    static String formatLogEventPayload(const String& payload)
    {
        String normalizedPayload = payload.trim();

        if (normalizedPayload.isEmpty() || normalizedPayload.equalsIgnoreCase("null"))
        {
            return "";
        }

        var parsed = JSON::parse(normalizedPayload);
        auto* dict = parsed.getDynamicObject();

        if (dict == nullptr)
        {
            return "";
        }

        String message = extractFirstNonEmptyField(dict, { "log", "message", "detail" });

        if (message.isEmpty())
        {
            return "";
        }

        String level =
            extractFirstNonEmptyField(dict, { "level", "type", "severity" }).toLowerCase();

        if (level.isEmpty())
        {
            level = "info";
        }

        return "[" + level + "] " + message;
    }

    static String formatProcessMessage(const String& messageType, const String& dataPayload = "")
    {
        if (messageType.isEmpty())
        {
            return "";
        }

        /* Gradio "log" event, emitted when the server calls gr.Info() or gr.Warning().

           NOTE: Gradio does not forward these on the "simple" /gradio_api/call
           endpoint that this client uses — its translation layer drops every
           message except process_completed, process_generating, heartbeat and
           unexpected_error (verified against Gradio 5.28 through 6.24). They are
           only delivered on the full queue API (/gradio_api/queue/join plus
           /gradio_api/queue/data), so this branch is currently unreachable and
           is kept for if/when this client moves to that API. */
        if (messageType.equalsIgnoreCase("log"))
        {
            return formatLogEventPayload(dataPayload);
        }

        if (messageType.equalsIgnoreCase("process_starts"))
        {
            return "[process] started";
        }

        if (messageType.equalsIgnoreCase("process_completed"))
        {
            return "[process] completed";
        }

        if (messageType.startsWithIgnoreCase("process_"))
        {
            String detail =
                messageType.fromFirstOccurrenceOf("process_", false, false).replace("_", " ");
            return "[process] " + detail;
        }

        if (messageType.equalsIgnoreCase("heartbeat") || messageType.equalsIgnoreCase("complete")
            || messageType.equalsIgnoreCase("error"))
        {
            return "";
        }

        return "[status] " + messageType.replace("_", " ");
    }

    // currentSseEventType: the value from the preceding "event: <type>" line, e.g. "log",
    //                       "process_starts", "heartbeat". Empty if no event: line preceded this.
    void appendProcessMessageFromDataLine(const String& dataLine,
                                          const String& currentSseEventType = "")
    {
        if (statusMessage == nullptr)
            return;

        // Heartbeats arrive continuously and never carry a user-facing message
        if (isEventType(currentSseEventType, GradioEvents::Heartbeat))
            return;

        String payload = extractPayload(dataLine);

        // Prefer the SSE event type from the preceding "event:" line (modern Gradio API).
        // Fall back to extracting a "msg" field from the JSON payload (old queue API).
        String messageType = currentSseEventType.isNotEmpty()
                                 ? currentSseEventType
                                 : extractMessageTypeFromPayload(payload);

        String statusText;

        if (messageType.isNotEmpty())
        {
            // For "log" events, pass the full data payload so we can extract the message text.
            statusText = formatProcessMessage(messageType, payload);
        }

        if (statusText.isEmpty())
        {
            statusText = extractGradioAlertStatusFromPayload(payload);
        }

        if (statusText.isEmpty())
            return;

        DBG_AND_LOG("GradioClient::appendProcessMessageFromDataLine: \""
                    << statusText << "\" (event type \"" << messageType << "\").");

        statusMessage->setMessage(statusText);
    }

    // String response version
    OpResult makeGETRequest(const URL endpoint,
                            String& response,
                            const String errorPath = "",
                            const int timeoutMs = -1,
                            const String modelPathForQuotaCheck = "")
    {
        std::unique_ptr<InputStream> stream;

        OpResult result = makeGETRequestStream(endpoint, stream, errorPath, timeoutMs);

        if (result.failed())
        {
            return result;
        }

        // Tracks whether any result-bearing data: lines have arrived. Heartbeat
        // and log data lines are excluded — they say nothing about whether the
        // job itself has produced output.
        bool seenResultData = false;
        bool checkedFirstLine = false;

        // Track the most recent SSE "event: <type>" line so we can pass it to
        // appendProcessMessageFromDataLine when the next "data:" line arrives.
        String currentSseEventType;

        while (! stream->isExhausted())
        {
            response = stream->readNextLine();

            DBG_AND_LOG("GradioClient::makeGETRequest: Streamed response \"" << response << "\".");

            String eventLine = response.trim();

            if (eventLine.isEmpty())
            {
                // Blank line signals the end of one SSE message block
                currentSseEventType = "";
                continue;
            }

            if (! checkedFirstLine)
            {
                checkedFirstLine = true;

                if (isHTMLResponse(eventLine))
                {
                    return OpResult::fail(HttpError {
                        HttpError::Type::BadStatusCode, HttpError::Request::GET, errorPath, 503 });
                }
            }

            // Capture the SSE event type from "event: <type>" lines.
            if (eventLine.startsWithIgnoreCase("event:"))
            {
                currentSseEventType = eventLine.fromFirstOccurrenceOf(":", false, false).trim();

                if (isEventType(currentSseEventType, GradioEvents::Complete))
                {
                    response = extractPayload(stream->readNextLine());

                    DBG_AND_LOG("GradioClient::makeGETRequest: Final response \"" << response
                                                                                  << "\".");

                    break;
                }
                else if (isEventType(currentSseEventType, GradioEvents::Error))
                {
                    String errorDataLine = stream->readNextLine();

                    DBG_AND_LOG("GradioClient::makeGETRequest: Error response \"" << errorDataLine
                                                                                  << "\".");

                    String payload = extractPayload(errorDataLine);
                    GradioErrorInfo errorInfo = extractErrorInfoFromPayload(payload);

                    String diagnosticText = errorInfo.combined();
                    String reason = extractShortReason(diagnosticText);

                    if (statusMessage != nullptr && reason.isNotEmpty())
                    {
                        statusMessage->setMessage("[error] " + reason);
                    }

                    if (isSpaceStatusError(diagnosticText))
                    {
                        return OpResult::fail(HttpError { HttpError::Type::BadStatusCode,
                                                          HttpError::Request::GET,
                                                          errorPath,
                                                          503 });
                    }

                    GradioError::Type errorType = GradioError::Type::RuntimeError;

                    if (diagnosticText.containsIgnoreCase("quota"))
                    {
                        /* ZeroGPU raises quota exhaustion as a gr.Error titled
                           "ZeroGPU quota exceeded", which Gradio >= 6.13 forwards
                           verbatim on this endpoint */
                        errorType = GradioError::Type::QuotaExceeded;
                    }
                    else if (errorInfo.payloadAbsent && ! seenResultData
                             && isZeroGPUSpace(modelPathForQuotaCheck))
                    {
                        /* Gradio <= 6.12 drops the error payload entirely, so a quota
                           rejection really is indistinguishable from any other early
                           failure. Report that ambiguity rather than guessing: the space
                           runs on ZeroGPU, so quota is one possible cause, but nothing
                           here establishes it.

                           Note this is only reached when NO payload arrived. A payload
                           that is present but carries no message (Gradio >= 6.13 with
                           show_error=False) is a genuine runtime error whose details the
                           server withheld, never a quota rejection, since gr.Error
                           messages are forwarded regardless of show_error. */
                        errorType = GradioError::Type::Indeterminate;
                    }

                    return OpResult::fail(GradioError { errorType, errorPath, reason });
                }
            }
            else if (eventLine.startsWithIgnoreCase("data:"))
            {
                if (! isEventType(currentSseEventType, GradioEvents::Heartbeat)
                    && ! currentSseEventType.equalsIgnoreCase("log"))
                {
                    seenResultData = true;
                }

                // Pass the current SSE event type so "log" events are not dropped.
                appendProcessMessageFromDataLine(eventLine, currentSseEventType);
            }
        }

        /* Falling out of the loop means the stream ended without ever delivering a
           "complete" or "error" event, so there is no response to parse. Reporting
           success here would leave the caller to fail on an empty string, which
           surfaces as a confusing JSON error rather than a connection problem.

           The most common cause is the transfer being aborted while the model was
           still working: JUCE's curl backend turns the timeout into a low-speed
           abort (CURLOPT_LOW_SPEED_LIMIT/TIME), and a stream carrying nothing but
           Gradio's periodic heartbeats sits far below that threshold. */
        return OpResult::fail(GradioError { GradioError::Type::IncompleteResponse, errorPath });
    }

    OpResult downloadFile(String downloadPath, File& fileToDownload) //override
    {
        // Obtain local temporary directory for downloaded file
        File tempDir = File::getSpecialLocation(File::tempDirectory);

        URL endpoint = URL(downloadPath);
        String fileName = endpoint.getFileName();
        String baseName =
            File::createFileWithoutCheckingPath(fileName).getFileNameWithoutExtension();

        String extension = File::createFileWithoutCheckingPath(fileName).getFileExtension();

        // Create file at a unique path
        fileToDownload = tempDir.getChildFile(baseName + "_" + Uuid().toString() + extension);

        OpResult result = makeGETRequest(endpoint, fileToDownload, downloadPath);

        if (result.failed())
        {
            return result;
        }

        return OpResult::ok();
    }

    // File download version
    OpResult makeGETRequest(const URL endpoint,
                            File& fileToDownload,
                            const String errorPath = "",
                            const int timeoutMs = -1)
    {
        std::unique_ptr<InputStream> stream;

        OpResult result = makeGETRequestStream(endpoint, stream, errorPath, timeoutMs);

        // Remove file at target path if one already exists
        fileToDownload.deleteFile();

        // Create output stream to save file locally
        std::unique_ptr<FileOutputStream> outputFileStream(fileToDownload.createOutputStream());

        if (! outputFileStream || ! outputFileStream->openedOk())
        {
            return OpResult::fail(
                FileError { FileError::Type::DownloadFailed, fileToDownload.getFullPathName() });
        }

        // Copy data from GET request stream to stream for output file
        outputFileStream->writeFromInputStream(*stream, stream->getTotalLength());

        return OpResult::ok();
    }

    OpResult makeRequest(const String modelPath,
                         const String requestType,
                         const String body,
                         String& response,
                         const int timeoutMs = controlsTimeoutMs)
    {
        URL endpoint = URL(inferEndpointPath(modelPath))
                           .getChildURL("gradio_api")
                           .getChildURL("call")
                           .getChildURL(requestType);

        OpResult result = makePOSTRequest(endpoint,
                                          getJSONHeaders(),
                                          body,
                                          response,
                                          inferDocumentationPath(modelPath),
                                          File(),
                                          10000,
                                          modelPath);

        if (result.failed())
        {
            return result;
        }

        DynamicObject::Ptr responseDict;

        result = stringJSONToDict(response, responseDict);

        if (result.failed())
        {
            return result;
        }

        static const Identifier eventKey { "event_id" };

        if (! responseDict->hasProperty(eventKey))
        {
            return OpResult::fail(JsonError { JsonError::Type::MissingKey,
                                              JSON::toString(var(responseDict.get()), true),
                                              eventKey.toString() });
        }

        String eventID = responseDict->getProperty(eventKey);

        DBG_AND_LOG("GradioClient::queryControls: Process created with ID \"" << eventID << "\".");

        endpoint = URL(inferEndpointPath(modelPath))
                       .getChildURL("gradio_api")
                       .getChildURL("call")
                       .getChildURL(requestType)
                       .getChildURL(eventID);

        response.clear();

        /* Note: it's very important to give Gradio enough time to yield a response
           (10 seconds was too little for ZeroGPU spaces and led to stream == nullptr) */
        return makeGETRequest(
            endpoint, response, inferDocumentationPath(modelPath), timeoutMs, modelPath);
    }

    OpResult extractLabels(DynamicObject::Ptr& output, LabelList& labels)
    {
        static const Identifier labelsKey { "labels" };

        Array<var>* labelObjects;

        OpResult result = getRequiredArrayProperty(output, labelsKey, labelObjects);

        if (result.failed())
        {
            return result;
        }

        for (int i = 0; i < labelObjects->size(); i++)
        {
            DynamicObject* labelObject = labelObjects->getReference(i).getDynamicObject();

            String labelType = labelObject->getProperty("label_type").toString();

            std::unique_ptr<OutputLabel> label;

            String labelLogMessage = "GradioClient::extractLabels: Extracted label of type \"";

            /* Cast to given label type */

            if (labelType == "AudioLabel")
            {
                labelLogMessage += "AudioLabel";

                auto audioLabel = std::make_unique<AudioLabel>();

                if (labelObject->hasProperty("amplitude"))
                {
                    if (labelObject->getProperty("amplitude").isDouble()
                        || labelObject->getProperty("amplitude").isInt())
                    {
                        audioLabel->amplitude =
                            static_cast<float>(labelObject->getProperty("amplitude"));
                    }
                }

                label = std::move(audioLabel);
            }
            else if (labelType == "SpectrogramLabel")
            {
                labelLogMessage += "SpectrogramLabel";

                auto spectrogramLabel = std::make_unique<SpectrogramLabel>();

                if (labelObject->hasProperty("frequency"))
                {
                    if (labelObject->getProperty("frequency").isDouble()
                        || labelObject->getProperty("frequency").isInt())
                    {
                        spectrogramLabel->frequency =
                            static_cast<float>(labelObject->getProperty("frequency"));
                    }
                }

                label = std::move(spectrogramLabel);
            }
            else if (labelType == "MidiLabel")
            {
                labelLogMessage += "MidiLabel";

                auto midiLabel = std::make_unique<MidiLabel>();

                if (labelObject->hasProperty("pitch"))
                {
                    if (labelObject->getProperty("pitch").isDouble()
                        || labelObject->getProperty("pitch").isInt())
                    {
                        midiLabel->pitch = static_cast<float>(labelObject->getProperty("pitch"));
                    }
                }

                label = std::move(midiLabel);
            }
            else if (labelType == "OutputLabel")
            {
                labelLogMessage += "OutputLabel";

                auto outputLabel = std::make_unique<OutputLabel>();

                label = std::move(outputLabel);
            }
            else
            {
                // Unsupported label type received
                jassertfalse;
            }

            labelLogMessage += "\" with time \"";

            /* Fill all provided struct properties */

            if (labelObject->hasProperty("t"))
            {
                // Make sure time was provided as a float
                if (labelObject->getProperty("t").isDouble()
                    || labelObject->getProperty("t").isInt())
                {
                    label->t = static_cast<float>(labelObject->getProperty("t"));

                    labelLogMessage += String(label->t);
                }
            }

            labelLogMessage += "\" and label \"";

            if (labelObject->hasProperty("label"))
            {
                // Make sure label was provided as a string
                if (labelObject->getProperty("label").isString())
                {
                    label->label = labelObject->getProperty("label").toString();

                    labelLogMessage += label->label;
                }
            }

            labelLogMessage += "\".";

            if (labelType == "SpectrogramLabel")
            {
                labelLogMessage += " WARNING: HARP does not yet support spectrogram visualization.";
            }

            DBG_AND_LOG(labelLogMessage);

            if (labelObject->hasProperty("duration"))
            {
                // Make sure duration was provided as a float
                if (labelObject->getProperty("duration").isDouble()
                    || labelObject->getProperty("duration").isInt())
                {
                    label->duration = static_cast<float>(labelObject->getProperty("duration"));
                }
            }

            if (labelObject->hasProperty("description"))
            {
                // Make sure description was provided as a string
                if (labelObject->getProperty("description").isString())
                {
                    label->description = labelObject->getProperty("description").toString();
                }
            }

            if (labelObject->hasProperty("color"))
            {
                // Make sure color was provided as a int
                if ((labelObject->getProperty("color").isInt64()
                     || labelObject->getProperty("color").isInt()))
                {
                    int color_val = static_cast<int>(labelObject->getProperty("color"));

                    if (color_val != 0)
                    {
                        label->color = color_val;
                    }
                }
            }

            if (labelObject->hasProperty("link"))
            {
                // Make sure link was provided as a string
                if (labelObject->getProperty("link").isString())
                {
                    label->link = labelObject->getProperty("link").toString();
                }
            }

            labels.push_back(std::move(label));
        }

        return OpResult::ok();
    }

    /* How long a request may go without meaningful traffic before the transfer is
       abandoned. Gradio only emits a heartbeat every 15 seconds while a model runs,
       so on the curl backend this acts as a ceiling on total processing time. */
    static constexpr int controlsTimeoutMs = 120000; // 2 minutes
    static constexpr int processTimeoutMs = 1800000; // 30 minutes

    // Cached results of isZeroGPUSpace, keyed by model path
    CriticalSection zeroGPUCacheLock;
    std::map<String, bool> zeroGPUCache;
};
