/**
 * @file Client.h
 * @brief Helper functions, shared functionality, and parent class for interacting with APIs.
 * @author cwitkowitz, VedMistry42, huiranyu, xribene
 */

#pragma once

#include <unordered_map>

#include <juce_core/juce_core.h>

#include "../utils/Enums.h"
#include "../utils/Errors.h"
#include "../utils/Labels.h"
#include "../utils/Logging.h"
#include "../utils/Messages.h"
#include "../utils/Providers.h"
#include "../utils/Settings.h"

using namespace juce;

/* The API key saved for each provider. Read by worker threads making requests while the
   settings tab may be changing it, so every access is locked. */
class SharedAPIKeys
{
public:
    // Prefix of the settings keys under which the API keys are saved
    static constexpr const char* settingsPrefix = "apikeys";

    SharedAPIKeys()
    {
        for (auto provider : { Provider::HuggingFace, Provider::Stability })
        {
            const String savedKey = Settings::getString(getSettingsKey(provider));

            if (savedKey.isNotEmpty())
            {
                keys[provider] = savedKey;
            }
        }
    }

    // The saved key, or an empty string if there is none
    String getKey(Provider provider) const
    {
        const ScopedLock lock(keysLock);

        auto it = keys.find(provider);

        return it != keys.end() ? it->second : String();
    }

    void updateKey(Provider provider, const String& newKey)
    {
        {
            const ScopedLock lock(keysLock);
            keys[provider] = newKey;
        }

        Settings::setValue(getSettingsKey(provider), newKey, true);
    }

    void removeKey(Provider provider)
    {
        {
            const ScopedLock lock(keysLock);
            keys.erase(provider);
        }

        Settings::removeValue(getSettingsKey(provider), true);
    }

private:
    static String getSettingsKey(Provider provider)
    {
        return String(settingsPrefix) + "." + enumToString(provider);
    }

    CriticalSection keysLock;

    std::unordered_map<Provider, String> keys;
};

inline OpResult parseJSONString(const String& stringJSON, var& outData)
{
    const Result result = JSON::parse(stringJSON, outData);

    if (result.failed())
    {
        return OpResult::fail(JsonError { JsonError::Type::InvalidJSON, stringJSON });
    }

    return OpResult::ok();
}

inline OpResult stringJSONToDict(const String& stringJSON, DynamicObject::Ptr& dict)
{
    var data;

    OpResult result = parseJSONString(stringJSON, data);

    if (result.failed())
    {
        return result;
    }

    if (! data.isObject())
    {
        return OpResult::fail(JsonError { JsonError::Type::NotADictionary, stringJSON });
    }

    dict = data.getDynamicObject();

    jassert(dict != nullptr);

    return OpResult::ok();
}

inline OpResult stringJSONToList(const String& stringJSON, Array<var>& list)
{
    var data;

    OpResult result = parseJSONString(stringJSON, data);

    if (result.failed())
    {
        return result;
    }

    if (! data.isArray())
    {
        return OpResult::fail(JsonError { JsonError::Type::NotAnArray, stringJSON });
    }

    list = *data.getArray();

    return OpResult::ok();
}

inline OpResult getRequiredDictProperty(DynamicObject::Ptr& parentDict,
                                        const Identifier& key,
                                        DynamicObject::Ptr& outDict)
{
    if (parentDict == nullptr)
    {
        return OpResult::fail(JsonError { JsonError::Type::NotADictionary, {} });
    }

    if (! parentDict->hasProperty(key))
    {
        return OpResult::fail(JsonError { JsonError::Type::MissingKey,
                                          JSON::toString(var(parentDict.get()), true),
                                          key.toString() });
    }

    const var& value = parentDict->getProperty(key);

    if (! value.isObject())
    {
        return OpResult::fail(JsonError { JsonError::Type::NotADictionary, value.toString() });
    }

    outDict = value.getDynamicObject();

    jassert(outDict != nullptr);

    return OpResult::ok();
}

inline OpResult getRequiredArrayProperty(DynamicObject::Ptr& parentDict,
                                         const Identifier& key,
                                         Array<var>*& outArray)
{
    if (parentDict == nullptr)
    {
        return OpResult::fail(JsonError { JsonError::Type::NotADictionary, {} });
    }

    if (! parentDict->hasProperty(key))
    {
        return OpResult::fail(JsonError { JsonError::Type::MissingKey,
                                          JSON::toString(var(parentDict.get()), true),
                                          key.toString() });
    }

    const var& value = parentDict->getProperty(key);

    if (! value.isArray())
    {
        return OpResult::fail(JsonError { JsonError::Type::NotAnArray, value.toString() });
    }

    outArray = value.getArray();

    return OpResult::ok();
}

/*
   Keeps track of the network streams that are currently in flight for a single
   model, so that they can be aborted locally.

   This is not the same thing as Client::cancel(), which asks the server to stop
   a job by sending it a brand new request: that leaves the original connection
   open and needs the network to be reachable. Aborting here closes the
   connection on this side, so a worker thread blocked on it returns promptly and
   the underlying OS networking task cannot deliver its completion long after the
   objects that started the request are gone.
*/
class RequestRegistry
{
public:
    void abortActiveRequests()
    {
        const ScopedLock lock(streamsLock);

        aborted = true;

        for (WebInputStream* stream : streams)
        {
            stream->cancel();
        }
    }

    bool hasBeenAborted() const
    {
        const ScopedLock lock(streamsLock);

        return aborted;
    }

    void addStream(WebInputStream* stream)
    {
        const ScopedLock lock(streamsLock);

        streams.add(stream);
    }

    void removeStream(WebInputStream* stream)
    {
        const ScopedLock lock(streamsLock);

        streams.removeFirstMatchingValue(stream);
    }

private:
    CriticalSection streamsLock;

    Array<WebInputStream*> streams;

    bool aborted = false;
};

/*
   A WebInputStream that stays registered with a RequestRegistry for as long as
   it exists, so the registry can abort it while a worker thread is blocked
   connecting to it or reading from it. Unregistering in the destructor (under
   the registry lock) guarantees the registry never holds a dangling stream.
*/
class RegisteredWebInputStream : public WebInputStream
{
public:
    RegisteredWebInputStream(RequestRegistry& registryToUse,
                             const URL& url,
                             bool addParametersToRequestBody)
        : WebInputStream(url, addParametersToRequestBody), registry(registryToUse)
    {
        registry.addStream(this);
    }

    ~RegisteredWebInputStream() override { registry.removeStream(this); }

private:
    RequestRegistry& registry;
};

/*
   Equivalent to URL::createInputStream(), except that the stream is
   registered with requestRegistry for its whole lifetime so that it can be
   aborted locally (see RequestRegistry). Registering before connecting is
   what makes the connect phase abortable too, since that phase blocks for up
   to the connection timeout.

   Returns nullptr if the connection could not be established, including
   when the request was aborted. Without a registry this is the same as
   URL::createInputStream().
*/
inline std::unique_ptr<InputStream> createRegisteredStream(RequestRegistry* requestRegistry,
                                                           const URL& endpoint,
                                                           const URL::InputStreamOptions& options)
{
    if (requestRegistry == nullptr || endpoint.isLocalFile())
    {
        return endpoint.createInputStream(options);
    }

    auto stream = std::make_unique<RegisteredWebInputStream>(
        *requestRegistry,
        endpoint,
        options.getParameterHandling() == URL::ParameterHandling::inPostData);

    // Checked once registered, so that an abort cannot slip in between the check and the
    // registration and leave this connection running
    if (requestRegistry->hasBeenAborted())
    {
        return nullptr;
    }

    const String extraHeaders = options.getExtraHeaders();

    if (extraHeaders.isNotEmpty())
    {
        stream->withExtraHeaders(extraHeaders);
    }

    const int connectionTimeoutMs = options.getConnectionTimeoutMs();

    if (connectionTimeoutMs != 0)
    {
        stream->withConnectionTimeout(connectionTimeoutMs);
    }

    const String requestCmd = options.getHttpRequestCmd();

    if (requestCmd.isNotEmpty())
    {
        stream->withCustomRequestCommand(requestCmd);
    }

    stream->withNumRedirectsToFollow(options.getNumRedirectsToFollow());

    const bool connected = stream->connect(nullptr);

    if (int* statusCode = options.getStatusCode())
    {
        *statusCode = stream->getStatusCode();
    }

    if (StringPairArray* responseHeaders = options.getResponseHeaders())
    {
        *responseHeaders = stream->getResponseHeaders();
    }

    if (! connected || stream->isError())
    {
        return nullptr;
    }

    return stream;
}

class Client
{
public:
    Client() = default;
    virtual ~Client() = default;

    void setRequestRegistry(RequestRegistry* registryToUse) { requestRegistry = registryToUse; }

    /**
     * Reduces the many ways of writing one model's address to a single form.
     *
     * A model can be entered abbreviated, as a full page URL, or as the API
     * subdomain, and all three should be recognized as the same model rather than
     * accumulating as separate entries. Returns the path unchanged when there is
     * no canonical form to reduce it to.
     */
    virtual String canonicalizePath(String modelPath) { return modelPath; }

    /**
     * Confirms a model's address with the provider and returns its exact spelling.
     *
     * Some of the addresses a model can be entered as cannot be reduced without
     * asking the provider, so this may make a network request and must not be
     * called from the message thread. Providers that need no such lookup keep the
     * default, which reduces the path offline and succeeds.
     */
    virtual OpResult resolveCanonicalPath(const String& modelPath, String& canonicalPath)
    {
        canonicalPath = canonicalizePath(modelPath);

        return OpResult::ok();
    }

    virtual String inferHostSlashModel(String modelPath) = 0;
    virtual String inferEndpointPath(String modelPath) = 0;
    virtual String inferDocumentationPath(String modelPath) = 0;

    // See createRegisteredStream
    std::unique_ptr<InputStream> createRequestStream(const URL& endpoint,
                                                     const URL::InputStreamOptions& options) const
    {
        return createRegisteredStream(requestRegistry, endpoint, options);
    }

    OpResult queryToken(const String& tokenToQuery, String& response, const int timeoutMs = 10000)
    {
        String tokenValidationPath = tokenValidationURL.toString(true);

        // Neither the key nor the response (which describes the account) belongs in the log
        DBG_AND_LOG("Client::queryToken: Validating an API key at \"" << tokenValidationPath
                                                                      << "\".");

        if (! tokenValidationURL.isWellFormed())
        {
            return fail(HttpError {
                HttpError::Type::InvalidURL, HttpError::Request::GET, tokenValidationPath });
        }

        int statusCode = 0;

        auto options = URL::InputStreamOptions(URL::ParameterHandling::inAddress)
                           .withExtraHeaders(getAuthorizationHeader(tokenToQuery))
                           .withConnectionTimeoutMs(timeoutMs)
                           .withStatusCode(&statusCode);

        std::unique_ptr<InputStream> stream(createRequestStream(tokenValidationURL, options));

        if (stream == nullptr)
        {
            return fail(HttpError {
                HttpError::Type::ConnectionFailed, HttpError::Request::GET, tokenValidationPath });
        }

        response = stream->readEntireStreamAsString();

        DBG_AND_LOG("Client::queryToken: Received status code \"" << String(statusCode) << "\".");

        if (statusCode != 200)
        {
            return fail(HttpError { HttpError::Type::BadStatusCode,
                                    HttpError::Request::GET,
                                    tokenValidationPath,
                                    statusCode });
        }

        return OpResult::ok();
    }

    virtual OpResult validateToken(const String& tokenToValidate)
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

        return OpResult::ok();
    }

    virtual OpResult queryControls(String modelPath, DynamicObject::Ptr& controls) = 0;

    virtual OpResult uploadFile(String modelPath, const File& fileToUpload, String& remoteFilePath)
    {
        ignoreUnused(modelPath);

        // By default, simply pass through original file
        remoteFilePath = fileToUpload.getFullPathName();

        return OpResult::ok();
    }

    virtual var wrapPayloadElement(var payloadElement, bool isFile = false, String label = "") = 0;

    virtual OpResult process(String modelPath,
                             String& payloadJSON,
                             std::vector<File>& outputFiles,
                             LabelList& labels) = 0;
    virtual OpResult cancel(String modelPath)
    {
        ignoreUnused(modelPath);
        return OpResult::ok();
    }

    const String emptyJSONBody = R"({"data": []})";

    String acceptHeader;
    String contentTypeJSONHeader;

    /* Escapes line breaks for single-line logging, and redacts any credentials.

       API keys must never reach the log file: HARP asks users to open their logs
       and to attach them to bug reports, so anything written here should be
       assumed to end up in a public issue. */
    String toPrintableHeaders(String headers)
    {
        String printableHeaders = headers.replace("\r", "\\r").replace("\n", "\\n");

        static const StringArray sensitivePrefixes { "Authorization:", "Cookie:", "Set-Cookie:" };

        for (const auto& prefix : sensitivePrefixes)
        {
            int searchFrom = 0;

            for (;;)
            {
                int prefixStart = printableHeaders.indexOfIgnoreCase(searchFrom, prefix);

                if (prefixStart < 0)
                {
                    break;
                }

                int valueStart = prefixStart + prefix.length();

                // Header values are separated by the escaped line breaks above
                int valueEnd = printableHeaders.indexOf(valueStart, "\\r");

                if (valueEnd < 0)
                {
                    valueEnd = printableHeaders.indexOf(valueStart, "\\n");
                }

                if (valueEnd < 0)
                {
                    valueEnd = printableHeaders.length();
                }

                printableHeaders = printableHeaders.replaceSection(
                    valueStart, valueEnd - valueStart, " <redacted>");

                searchFrom = valueStart;
            }
        }

        return printableHeaders;
    }

    Provider provider;

    URL tokenValidationURL;
    URL tokenRegistrationURL;

protected:
    /* The headers of a request. A client whose endpoints are not all run by its provider
       leaves out the API key for those, which would otherwise be handed to whoever runs
       them. */
    String getCommonHeaders(bool includeAPIKey = true) const
    {
        return (includeAPIKey ? getAuthorizationHeader() : String()) + acceptHeader
               + getClientHeader();
    }

    String getJSONHeaders(bool includeAPIKey = true) const
    {
        return getCommonHeaders(includeAPIKey) + contentTypeJSONHeader;
    }

    SharedResourcePointer<StatusMessage> statusMessage;

    /* A new file in the temporary directory for a result, named after it and stamped with
       the time it arrived, e.g. "open-unmix_vocals_10-20-2026_14-35-12.wav". HARP shows a
       result under its file name, so a time keeps the results of successive runs apart in a
       form a user can read. Results named alike in the same second are numbered. */
    static File createResultFile(const String& baseName, const String& extension)
    {
        const String stamped =
            baseName + "_" + Time::getCurrentTime().formatted("%m-%d-%Y_%H-%M-%S");
        const File directory = File::getSpecialLocation(File::tempDirectory);

        // Numbered " (2)", " (3)", and so on
        File file = directory.getChildFile(stamped + extension);

        for (int number = 2; file.exists(); ++number)
            file = directory.getChildFile(stamped + " (" + String(number) + ")" + extension);

        // Claimed at once, so that a concurrent download cannot be given the same name
        file.create();

        return file;
    }

    /* Records which provider an error came from on its way out, the client being the
       only layer that knows. Overloaded rather than templated so that error types
       carrying no provider cannot be passed here by mistake. */
    OpResult fail(ClientError error) const
    {
        error.provider = provider;

        return OpResult::fail(error);
    }

    OpResult fail(HttpError error) const
    {
        error.provider = provider;

        return OpResult::fail(error);
    }

private:
    String getAuthorizationHeader() const
    {
        return getAuthorizationHeader(sharedTokens->getKey(provider));
    }

    /* Identifies this client to a PyHARP endpoint, so that a cancel it sends stops the
       job it started rather than whatever happens to be running. Gradio scopes its own
       cancellation to a browser session, which an endpoint reached over the API does not
       have, so the endpoint matches on this instead. One client is created per model
       load, which is the granularity to which a cancel belongs. */
    String getClientHeader() const { return "X-HARP-Client: " + clientId + "\r\n"; }

    String getAuthorizationHeader(String accessToken) const
    {
        String authorizationHeader;

        if (! accessToken.isEmpty())
        {
            authorizationHeader = "Authorization: Bearer " + accessToken + "\r\n";
        }

        return authorizationHeader;
    }

    const String clientId { Uuid().toDashedString() };

    SharedResourcePointer<SharedAPIKeys> sharedTokens;

    /* Owned by whatever the requests belong to, such as a Model. Without one, requests
       cannot be aborted. */
    RequestRegistry* requestRegistry = nullptr;
};
