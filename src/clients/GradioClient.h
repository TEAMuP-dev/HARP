/**
 * @file GradioClient.h
 * @brief Client specifics for Gradio and Hugging Face (simple JSON requests).
 * @author cwitkowitz, saumya-pailwan, xribene, huiranyu
 */

#pragma once

#include <map>

#include "Client.h"

using namespace juce;

class GradioClient : public Client
{
public:
    // Events of the simple API that the response handling depends on
    static constexpr const char* completeEvent = "complete";
    static constexpr const char* heartbeatEvent = "heartbeat";
    static constexpr const char* errorEvent = "error";

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
        modelPath = withoutTrailingSlashes(modelPath);

        return isValidLocalPath(modelPath) || isValidGradioPath(modelPath)
               || isValidHuggingFacePath(modelPath);
    }

    /* A pasted address often ends in a "/", which none of the forms below carries. The
       validators compare against the end of the path, so it is dropped before anything
       looks at the path, and the canonical path never has one. */
    static String withoutTrailingSlashes(String modelPath)
    {
        modelPath = modelPath.trim();

        while (modelPath.endsWithChar('/'))
            modelPath = modelPath.dropLastCharacters(1);

        return modelPath;
    }

    String canonicalizePath(String modelPath) override
    {
        modelPath = withoutTrailingSlashes(modelPath);

        /* "user/name" is the form Hugging Face itself uses, and the only one both
           the page URL and the API subdomain can be derived from. A page URL carries
           that form verbatim, so it can be reduced here without asking anyone.

           The API subdomain cannot: it lowercases the name and replaces both the
           separating "/" and every "_" with "-", so neither the boundary nor the
           original spelling survives. Reducing it offline would only guess, which is
           what resolveCanonicalPath below asks the Hub to avoid. Local and Gradio
           addresses have no canonical form at all and are left alone. */
        if (isValidLongHuggingFacePath(modelPath))
        {
            const String hostSlashModel = inferHostSlashModel(modelPath);

            if (hostSlashModel.isNotEmpty())
            {
                return hostSlashModel;
            }
        }

        return modelPath;
    }

    /**
     * Asks the provider for a model's exact address.
     *
     * The Hub answers for the two written forms and the running app answers for the
     * API subdomain, so an address that only appears to work (e.g., a "-" typed where the
     * name has a "_" or the wrong capitalization) is corrected here rather than
     * silently loading a different model or producing a broken documentation link.
     */
    OpResult resolveCanonicalPath(const String& modelPath, String& canonicalPath) override
    {
        canonicalPath = canonicalizePath(modelPath);

        // Only Hugging Face publishes a canonical spelling to resolve against
        if (! isValidHuggingFacePath(canonicalPath))
        {
            return OpResult::ok();
        }

        /* A subdomain has to be asked for its own identity, since the Hub cannot be
           queried for it without already knowing the answer. It advertises that
           identity in a "canonical" link header, which it sends whether it is awake
           or asleep, so no request to the app itself is needed. */
        if (isValidShortHuggingFacePath(canonicalPath))
        {
            const String advertisedPath = queryCanonicalSpace(inferEndpointPath(canonicalPath));

            if (advertisedPath.isNotEmpty())
            {
                if (advertisedPath != canonicalPath)
                {
                    DBG_AND_LOG("GradioClient::resolveCanonicalPath: Path \""
                                << modelPath << "\" resolved to \"" << advertisedPath << "\".");
                }

                canonicalPath = advertisedPath;
            }
            else
            {
                /* Without an answer only the ambiguous reading of the subdomain is
                   left, which is no basis for rejecting the path. */
                DBG_AND_LOG("GradioClient::resolveCanonicalPath: Path \""
                            << modelPath << "\" advertises no identity. Continuing with \""
                            << canonicalPath << "\".");
            }

            return OpResult::ok();
        }

        static const Identifier identityKey { "id" };

        std::unique_ptr<InputStream> stream;

        OpResult result = makeGETRequestStream(
            URL("https://huggingface.co/api/spaces/" + canonicalPath), stream, modelPath, 10000);

        if (result.failed())
        {
            const auto* e = std::get_if<HttpError>(&result.getError());

            /* 401 and 404 are the Hub's answers for "no such Space" and for "private,
               and you are not signed in", which it does not distinguish between. Both
               mean this address cannot be loaded, so say so now with a message about
               the address rather than letting the request fail later as a transport
               error. */
            if (e != nullptr && e->type == HttpError::Type::BadStatusCode
                && (e->statusCode == 401 || e->statusCode == 404))
            {
                DBG_AND_LOG("GradioClient::resolveCanonicalPath: Path \""
                            << modelPath << "\" was not recognized by the provider (status code \""
                            << String(e->statusCode) << "\").");

                return fail(ClientError { ClientError::Type::ModelNotFound, modelPath });
            }

            /* Anything else is a problem reaching the provider rather than a verdict
               on the path, and must not stop a load that would otherwise succeed. */
            DBG_AND_LOG("GradioClient::resolveCanonicalPath: Could not confirm path \""
                        << modelPath << "\" (" << toLogString(result.getError())
                        << "). Continuing with \"" << canonicalPath << "\".");

            return OpResult::ok();
        }

        DynamicObject::Ptr responseDict;

        if (stream == nullptr
            || stringJSONToDict(stream->readEntireStreamAsString(), responseDict).failed()
            || ! responseDict->hasProperty(identityKey))
        {
            DBG_AND_LOG("GradioClient::resolveCanonicalPath: Provider gave no identity for path \""
                        << modelPath << "\". Continuing with \"" << canonicalPath << "\".");

            return OpResult::ok();
        }

        const String resolvedPath = responseDict->getProperty(identityKey).toString().trim();

        if (resolvedPath.isNotEmpty() && isValidAbbrevHuggingFacePath(resolvedPath))
        {
            if (resolvedPath != canonicalPath)
            {
                DBG_AND_LOG("GradioClient::resolveCanonicalPath: Path \""
                            << modelPath << "\" resolved to \"" << resolvedPath << "\".");
            }

            canonicalPath = resolvedPath;
        }

        return OpResult::ok();
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
                /* Only the first "-" is known to be the owner boundary. The rest
                   belong to the name, whose original spelling the subdomain does not
                   preserve. This is a best guess for display and for addressing the
                   Hub, which resolveCanonicalPath replaces with the exact spelling. */
                const String subdomain = modelPath.fromFirstOccurrenceOf("https://", false, false)
                                             .upToFirstOccurrenceOf(".hf.space", false, false);

                hostSlashModel = subdomain.upToFirstOccurrenceOf("-", false, false) + "/"
                                 + subdomain.fromFirstOccurrenceOf("-", false, false);
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

                endpointPath = "https://" + host + "-" + model.replace("_", "-") + ".hf.space/";
            }
        }
        else
        {
            DBG_AND_LOG("GradioClient::inferEndpointPath: Path \""
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

    OpResult validateToken(const String& tokenToValidate) override
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

        const var tokenInfo = responseDict->getProperty("auth")["accessToken"];

        if (! tokenInfo.isObject())
        {
            return OpResult::fail(JsonError { JsonError::Type::MissingKey, {}, "accessToken" });
        }

        const String role = tokenInfo["role"].toString();

        // A classic read or write key grants everything HARP needs
        if (role == "write" || role == "read")
        {
            return OpResult::ok();
        }

        // A fine-grained key has to grant it within one of its scopes

        if (const auto* scopes = tokenInfo["fineGrained"]["scoped"].getArray())
        {
            for (const auto& scope : *scopes)
            {
                const auto* permissions = scope["permissions"].getArray();

                if (permissions != nullptr && permissions->contains("repo.content.read")
                    && permissions->contains("repo.write")
                    && permissions->contains("inference.serverless.write")
                    && permissions->contains("inference.endpoints.infer.write"))
                {
                    return OpResult::ok();
                }
            }
        }

        return fail(ClientError { ClientError::Type::InsufficientPermissions });
    }

    OpResult queryControls(String modelPath, DynamicObject::Ptr& controls) override
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
                                          getCommonHeaders(acceptsAPIKey(endpoint)),
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

        if (isFile && ! payloadElement.isVoid())
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
                     LabelList& labels) override
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
                    // Possibly from a newer pyharp, so it is left out rather than failing
                    DBG_AND_LOG("GradioClient::process: Ignoring output of unknown type \""
                                << outputType << "\".");
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

    OpResult cancel(String modelPath) override
    {
        String response;

        return makeRequest(modelPath, "cancel", emptyJSONBody, response);
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

    /* Whether an endpoint may be sent the Hugging Face API key. Hugging Face and this
       machine are trusted with it, and anything else (e.g., a gradio.live link or another
       computer on the network) is run by someone who has no use for it. */
    static bool acceptsAPIKey(const URL& endpoint)
    {
        const String host = endpoint.getDomain().toLowerCase();

        return host == "huggingface.co" || host.endsWith(".hf.space") || host == "localhost"
               || host == "127.0.0.1";
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

        /* Without this the check below is meaningless for any other address:
           upToFirstOccurrenceOf returns the whole string when the needle is
           absent, so a long-form path that happens to contain a single hyphen
           splits into two tokens and is mistaken for a subdomain. */
        if (! modelPath.contains(".hf.space"))
        {
            return false;
        }

        const String subdomain = modelPath.fromFirstOccurrenceOf("https://", false, false)
                                     .upToFirstOccurrenceOf(".hf.space", false, false);

        /* The owner and the name are joined by a "-", so at least one is needed for
           this to name a Space at all. Where that "-" falls, and which of the others
           were underscores, cannot be told from the address. resolveCanonicalPath
           asks the running app which Space it is rather than guessing. */
        return subdomain.isNotEmpty() && subdomain.containsChar('-');
    }

    static bool isValidLongHuggingFacePath(String modelPath)
    {
        /*
          e.g., "https://huggingface.co/spaces/xribene/midi_pitch_shifter"
        */

        if (! modelPath.startsWith("https://huggingface.co/spaces/"))
        {
            return false;
        }

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

    /* The Hub's report on how a Space is currently deployed, covering both its
       lifecycle stage and its hardware. Returns false when it cannot be obtained,
       which the callers treat as "unknown" rather than as a verdict. */
    bool querySpaceRuntime(const String& modelPath, DynamicObject::Ptr& runtime)
    {
        if (! isValidHuggingFacePath(modelPath))
            return false;

        URL runtimeEndpoint =
            URL("https://huggingface.co/api/spaces/" + inferHostSlashModel(modelPath) + "/runtime");

        std::unique_ptr<InputStream> stream;

        if (makeGETRequestStream(runtimeEndpoint, stream, "", 5000).failed() || stream == nullptr)
            return false;

        return stringJSONToDict(stream->readEntireStreamAsString(), runtime).wasOk();
    }

    /* The lifecycle stage reported by the Hub, e.g. RUNNING, SLEEPING, APP_STARTING,
       BUILDING, RUNTIME_ERROR, PAUSED. Returns an empty string when it cannot be
       determined. Unlike the hardware type this changes constantly, so it is never
       cached. */
    String querySpaceStage(const String& modelPath)
    {
        DynamicObject::Ptr runtime;

        if (! querySpaceRuntime(modelPath, runtime))
            return {};

        static const Identifier stageKey { "stage" };

        return runtime->hasProperty(stageKey)
                   ? runtime->getProperty(stageKey).toString().toUpperCase()
                   : String();
    }

    /* Which of the two verdicts a stage amounts to. Asking the Hub for the stage is
       what separates them, since a Space that is merely waking up answers a request
       exactly like one that is genuinely broken. A stage matching neither is left
       unclassified so that the original failure is reported as it stands. */
    static bool isStartingStage(const String& stage)
    {
        /* RUNNING belongs here because the Hub marks a Space running as soon as its
           container is up, a moment before the app behind it accepts requests. */
        return stage == "SLEEPING" || stage == "APP_STARTING" || stage == "BUILDING"
               || stage == "RUNNING";
    }

    static bool isUnavailableStage(const String& stage)
    {
        return stage == "RUNTIME_ERROR" || stage == "BUILD_ERROR" || stage == "CONFIG_ERROR"
               || stage == "NO_APP_FILE" || stage == "PAUSED" || stage == "STOPPED"
               || stage == "DELETING";
    }

    /* The exact "user/Name" a Space host answers for, taken from the "canonical"
       link header it sends alongside its page. The address used to reach a Space
       lowercases its name and replaces every "_" with "-", so this header is the
       only way to recover the original spelling. Returns an empty string when the
       host does not answer or does not advertise one. */
    String queryCanonicalSpace(const String& hostURL)
    {
        const URL host { hostURL };

        if (! host.isWellFormed())
            return {};

        StringPairArray responseHeaders;

        auto options = URL::InputStreamOptions(URL::ParameterHandling::inAddress)
                           .withExtraHeaders(getCommonHeaders(acceptsAPIKey(host)))
                           .withConnectionTimeoutMs(10000)
                           .withResponseHeaders(&responseHeaders)
                           .withNumRedirectsToFollow(5);

        /* A sleeping Space answers with an error status but still sends the header,
           so the status code is deliberately not consulted here. */
        if (createRequestStream(host, options) == nullptr)
            return {};

        static const String spacesPrefix { "https://huggingface.co/spaces/" };

        /* Several link headers can be sent at once and are joined into one value, so
           the canonical entry is picked out rather than the header taken whole. */
        for (const String& entry : StringArray::fromTokens(responseHeaders["link"], ",", "\"'"))
        {
            if (! entry.contains("canonical") || ! entry.contains(spacesPrefix))
                continue;

            const String advertised = entry.fromFirstOccurrenceOf(spacesPrefix, false, false)
                                          .upToFirstOccurrenceOf(">", false, false)
                                          .trim();

            if (isValidAbbrevHuggingFacePath(advertised))
                return advertised;
        }

        return {};
    }

    bool isZeroGPUSpace(const String& modelPath)
    {
        if (! isValidHuggingFacePath(modelPath))
            return false;

        // The hardware type is a static property of the Space, so it is cached to
        // avoid repeated blocking lookups from error-handling paths
        {
            const ScopedLock lock(zeroGPUCacheLock);

            auto cached = zeroGPUCache.find(modelPath);

            if (cached != zeroGPUCache.end())
                return cached->second;
        }

        DynamicObject::Ptr runtime;

        // Failed lookups are not cached, so that a transient network error
        // does not permanently misclassify the Space
        if (! querySpaceRuntime(modelPath, runtime))
            return false;

        // Navigate: hardware -> current
        static const Identifier hardwareKey { "hardware" };
        static const Identifier currentKey { "current" };

        String hardwareCurrent;

        if (runtime->hasProperty(hardwareKey))
        {
            if (auto* hardwareDict = runtime->getProperty(hardwareKey).getDynamicObject())
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
            return fail(
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

        std::unique_ptr<InputStream> stream(createRequestStream(endpoint, options));

        if (stream == nullptr)
        {
            return fail(HttpError {
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
                        GradioError { GradioError::Type::SpaceStarting, errorPath, stage, true });
                }

                if (isUnavailableStage(stage))
                {
                    return OpResult::fail(GradioError {
                        GradioError::Type::SpaceUnavailable, errorPath, stage, true });
                }
            }

            // Keep the real status code so callers can react to specific
            // failures (e.g. 404 for an invalid path, 402 for quota limits)
            // and attach any diagnostic text from the response body
            return fail(HttpError { HttpError::Type::BadStatusCode,
                                    HttpError::Request::POST,
                                    errorPath,
                                    statusCode,
                                    reason });
        }

        if (isHTMLResponse(response))
        {
            // A 200 with an HTML body is not a Gradio API response at all
            return fail(HttpError {
                HttpError::Type::UnexpectedResponse, HttpError::Request::POST, errorPath });
        }

        return OpResult::ok();
    }

    OpResult makeGETRequestStream(const URL endpoint,
                                  std::unique_ptr<InputStream>& stream,
                                  const String errorPath = "",
                                  const int timeoutMs = -1)
    {
        String requestHeaders = getCommonHeaders(acceptsAPIKey(endpoint));

        DBG_AND_LOG(
            "GradioClient::makeGETRequestStream: Attempting to make GET request for endpoint \""
            << endpoint.toString(true) << "\" with headers \"" << toPrintableHeaders(requestHeaders)
            << "\".");

        if (! endpoint.isWellFormed())
        {
            return fail(
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

        stream = createRequestStream(endpoint, options);

        if (stream == nullptr)
        {
            return fail(HttpError {
                HttpError::Type::ConnectionFailed, HttpError::Request::GET, errorPath });
        }

        DBG_AND_LOG("GradioClient::makeGETRequestStream: Received status code \""
                    << String(statusCode) << "\" and response \""
                    << toPrintableHeaders(responseHeaders.getDescription()) << "\".");

        if (statusCode != 200)
        {
            return fail(HttpError {
                HttpError::Type::BadStatusCode, HttpError::Request::GET, errorPath, statusCode });
        }

        return OpResult::ok();
    }

    static String extractPayload(const String& response)
    {
        String payload = response.trim();

        if (payload.startsWith("data:"))
        {
            payload = payload.fromFirstOccurrenceOf("data:", false, false).trim();
        }

        return payload;
    }

    // Compares an event type (the text after "event:") to a known event, ignoring case
    static bool isEventType(const String& eventType, const char* event)
    {
        return eventType.equalsIgnoreCase(event);
    }

    /* What a Gradio error payload carries.

       Gradio >= 6.13 forwards the failing event's error payload on this endpoint:
         {"error": "<message>", "title": "<title>", "duration": .., "visible": ..}
       The message is populated whenever the server raised a gr.Error (which is how
       ZeroGPU reports quota exhaustion, with title "ZeroGPU quota exceeded"), and
       for any other exception only when the app was launched with show_error=True.
       When it was not, the payload is {"error": null}, which is a real runtime
       error whose details the server deliberately withheld.

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

    /* Whether a data line carries nothing at all. Gradio writes an absent payload as
       a bare "null", so that has to be told apart from text worth parsing. */
    static bool isPayloadAbsent(const String& payload)
    {
        String normalizedPayload = payload.trim();

        return normalizedPayload.isEmpty() || normalizedPayload.equalsIgnoreCase("null")
               || normalizedPayload.equalsIgnoreCase("none");
    }

    static GradioErrorInfo extractErrorInfoFromPayload(const String& payload)
    {
        GradioErrorInfo info;

        if (isPayloadAbsent(payload))
        {
            info.payloadAbsent = true;

            return info;
        }

        String normalizedPayload = payload.trim();

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

    // Returns true when the Hugging Face proxy reports that the Space itself is broken
    static bool isSpaceStatusError(const String& text)
    {
        String lower = text.toLowerCase();

        return lower.contains("space is in error") || lower.contains("check its status on hf");
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

    /* Reports an event that is still in progress in the status area.

       The simple API (/gradio_api/call) delivers only "complete", "generating",
       "heartbeat", and "error" events, and drops the messages gr.Info and gr.Warning
       send (verified against Gradio 5.28 through 6.24). Those are delivered only on the
       queue API (/gradio_api/queue/join and /gradio_api/queue/data). */
    void reportEvent(const String& eventType)
    {
        if (eventType.isEmpty() || isEventType(eventType, heartbeatEvent)
            || isEventType(eventType, completeEvent) || isEventType(eventType, errorEvent))
        {
            return;
        }

        const String statusText = "[status] " + eventType.replace("_", " ");

        DBG_AND_LOG("GradioClient::reportEvent: \"" << statusText << "\".");

        statusMessage->setMessage(statusText);
    }

    // String response version
    OpResult makeGETRequest(const URL endpoint,
                            String& response,
                            const String errorPath = "",
                            const int timeoutMs = -1,
                            const String modelPath = "")
    {
        std::unique_ptr<InputStream> stream;

        OpResult result = makeGETRequestStream(endpoint, stream, errorPath, timeoutMs);

        if (result.failed())
        {
            return result;
        }

        // Tracks whether any result-bearing data: lines have arrived. Heartbeats are
        // excluded, since they say nothing about whether the job has produced output.
        bool seenResultData = false;
        bool checkedFirstLine = false;

        // Set when a "complete" event delivers the response the caller is waiting for
        bool receivedCompleteEvent = false;

        // The type from the most recent "event:" line, which labels the "data:" lines after it
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
                    return fail(HttpError {
                        HttpError::Type::UnexpectedResponse, HttpError::Request::GET, errorPath });
                }
            }

            // Capture the SSE event type from "event: <type>" lines.
            if (eventLine.startsWithIgnoreCase("event:"))
            {
                currentSseEventType = eventLine.fromFirstOccurrenceOf(":", false, false).trim();

                if (isEventType(currentSseEventType, completeEvent))
                {
                    response = extractPayload(stream->readNextLine());

                    DBG_AND_LOG("GradioClient::makeGETRequest: Final response \"" << response
                                                                                  << "\".");

                    receivedCompleteEvent = true;

                    break;
                }
                else if (isEventType(currentSseEventType, errorEvent))
                {
                    String errorDataLine = stream->readNextLine();

                    DBG_AND_LOG("GradioClient::makeGETRequest: Error response \"" << errorDataLine
                                                                                  << "\".");

                    String payload = extractPayload(errorDataLine);
                    GradioErrorInfo errorInfo = extractErrorInfoFromPayload(payload);

                    String diagnosticText = errorInfo.combined();
                    String reason = extractShortReason(diagnosticText);

                    /* The proxy has stated outright that the Space is down, so unlike
                       the ambiguous 503 handled in makePOSTRequest there is nothing to
                       decide. The Hub is still asked for the stage, purely so that the
                       message can name it, and only a stage that agrees is reported. */
                    if (isSpaceStatusError(diagnosticText))
                    {
                        const String stage = querySpaceStage(modelPath);

                        DBG_AND_LOG("GradioClient::makeGETRequest: Space stage \"" << stage
                                                                                   << "\".");

                        return OpResult::fail(
                            GradioError { GradioError::Type::SpaceUnavailable,
                                          errorPath,
                                          isUnavailableStage(stage) ? stage : String(),
                                          true });
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
                             && isZeroGPUSpace(modelPath))
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

                    return OpResult::fail(GradioError {
                        errorType, errorPath, reason, isValidHuggingFacePath(modelPath) });
                }
            }
            else if (eventLine.startsWithIgnoreCase("data:"))
            {
                if (! isEventType(currentSseEventType, heartbeatEvent))
                {
                    seenResultData = true;
                }

                reportEvent(currentSseEventType);
            }
        }

        if (! receivedCompleteEvent)
        {
            /* The stream ended without ever delivering a "complete" or "error" event,
               so there is no response to parse. Reporting success here would leave the
               caller to fail on an empty string, which surfaces as a confusing JSON
               error rather than the connection problem it is.

               The most common cause is the transfer being aborted while the model was
               still working: JUCE's curl backend turns the timeout into a low-speed
               abort (CURLOPT_LOW_SPEED_LIMIT/TIME), and a stream carrying nothing but
               Gradio's periodic heartbeats sits far below that threshold. */
            return OpResult::fail(GradioError { GradioError::Type::IncompleteResponse,
                                                errorPath,
                                                "",
                                                isValidHuggingFacePath(modelPath) });
        }

        return OpResult::ok();
    }

    OpResult downloadFile(String downloadPath, File& fileToDownload)
    {
        URL endpoint = URL(downloadPath);
        String fileName = endpoint.getFileName();
        String baseName =
            File::createFileWithoutCheckingPath(fileName).getFileNameWithoutExtension();

        String extension = File::createFileWithoutCheckingPath(fileName).getFileExtension();

        fileToDownload = createResultFile(baseName, extension);

        return makeGETRequest(endpoint, fileToDownload, downloadPath);
    }

    // File download version
    OpResult makeGETRequest(const URL endpoint,
                            File& fileToDownload,
                            const String errorPath = "",
                            const int timeoutMs = -1)
    {
        std::unique_ptr<InputStream> stream;

        OpResult result = makeGETRequestStream(endpoint, stream, errorPath, timeoutMs);

        // Nothing was opened to copy from (e.g., the request failed or was aborted), so
        // report why rather than reading a null stream
        if (result.failed())
        {
            return result;
        }

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
        const URL callEndpoint = URL(inferEndpointPath(modelPath))
                                     .getChildURL("gradio_api")
                                     .getChildURL("call")
                                     .getChildURL(requestType);

        OpResult result = makePOSTRequest(callEndpoint,
                                          getJSONHeaders(acceptsAPIKey(callEndpoint)),
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

        const String eventID = responseDict->getProperty(eventKey);

        DBG_AND_LOG("GradioClient::makeRequest: Created " << requestType << " event \"" << eventID
                                                          << "\".");

        response.clear();

        /* Gradio needs enough time to yield a response (10 seconds was too little for
           ZeroGPU Spaces and left the stream empty) */
        return makeGETRequest(callEndpoint.getChildURL(eventID),
                              response,
                              inferDocumentationPath(modelPath),
                              timeoutMs,
                              modelPath);
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

            if (labelObject == nullptr)
            {
                DBG_AND_LOG(
                    "GradioClient::extractLabels: Ignoring label that is not a dictionary.");

                continue;
            }

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
                // Possibly from a newer pyharp, so it is left out rather than failing
                DBG_AND_LOG("GradioClient::extractLabels: Ignoring label of unknown type \""
                            << labelType << "\".");

                continue;
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
                // Make sure color was provided as an int
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
