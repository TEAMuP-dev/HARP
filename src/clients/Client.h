/**
 * @file Client.h
 * @brief Helper functions, shared functionality, and parent class for interacting with APIs.
 * @author xribene, huiranyu, cwitkowitz
 */

#pragma once

#include <unordered_map>

#include <juce_core/juce_core.h>

#include "../utils/Enums.h"
#include "../utils/Errors.h"
#include "../utils/Labels.h"
#include "../utils/Logging.h"
#include "../utils/Settings.h"

using namespace juce;

// TODO - hard-coded client strings for error-reporting
//        can be deterministic based off of these enums
enum class Provider
{
    HuggingFace,
    Stability
};

struct SharedAPIKeys
{
    void initializeAPIKeys()
    {
        for (auto provider : { Provider::HuggingFace, Provider::Stability })
        {
            String savedToken = Settings::getString(providerToSettingsKey(provider));

            if (savedToken.isNotEmpty())
            {
                savedTokens[provider] = savedToken;
            }
        }
    }

    String providerToSettingsKey(Provider p) { return settingsPrefix + "." + enumToString(p); }

    void updateKey(Provider provider, String newAPIKey)
    {
        savedTokens[provider] = newAPIKey;

        Settings::setValue(providerToSettingsKey(provider), newAPIKey, true);
    }

    void removeKey(Provider provider)
    {
        savedTokens.erase(provider);

        Settings::removeValue(providerToSettingsKey(provider), true);
    }

    String settingsPrefix = "apikeys";

    std::unordered_map<Provider, String> savedTokens = {};
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

class Client
{
public:
    Client() = default;
    virtual ~Client() = default;

    void setRequestRegistry(RequestRegistry* registryToUse) { requestRegistry = registryToUse; }

    virtual String inferHostSlashModel(String modelPath) = 0;
    virtual String inferEndpointPath(String modelPath) = 0;
    virtual String inferDocumentationPath(String modelPath) = 0;

    /*
       Equivalent to URL::createInputStream(), except that the stream is
       registered with requestRegistry for its whole lifetime so that it can be
       aborted locally (see RequestRegistry). Registering before connecting is
       what makes the connect phase abortable too - that phase blocks for up to
       the connection timeout, which is two minutes for process requests.

       Returns nullptr if the connection could not be established, including
       when the request was aborted. Clients without a registry (e.g. the
       short-lived one used for token validation) behave exactly as before.
    */
    std::unique_ptr<InputStream> createRequestStream(const URL& endpoint,
                                                     const URL::InputStreamOptions& options) const
    {
        if (requestRegistry == nullptr || endpoint.isLocalFile())
        {
            return endpoint.createInputStream(options);
        }

        if (requestRegistry->hasBeenAborted())
        {
            // Requests were aborted, so do not open another connection
            return nullptr;
        }

        auto stream = std::make_unique<RegisteredWebInputStream>(
            *requestRegistry,
            endpoint,
            options.getParameterHandling() == URL::ParameterHandling::inPostData);

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

    OpResult queryToken(const String& tokenToQuery, String& response, const int timeoutMs = 10000)
    {
        String tokenValidationPath = tokenValidationURL.toString(true);

        DBG_AND_LOG("Client::queryToken: Attempting to query client at \""
                    << tokenValidationPath << "\" with token \"" << tokenToQuery << "\".");

        if (! tokenValidationURL.isWellFormed())
        {
            return OpResult::fail(HttpError {
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
            return OpResult::fail(HttpError {
                HttpError::Type::ConnectionFailed, HttpError::Request::GET, tokenValidationPath });
        }

        response = stream->readEntireStreamAsString();

        DBG_AND_LOG("Client::queryToken: Received status code \""
                    << String(statusCode) << "\" with response \"" << response << "\".");

        if (statusCode != 200)
        {
            return OpResult::fail(HttpError { HttpError::Type::BadStatusCode,
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

    String toPrintableHeaders(String headers)
    {
        return headers.replace("\r", "\\r").replace("\n", "\\n");
    }

    Provider provider;

    URL tokenValidationURL;
    URL tokenRegistrationURL;

protected:
    String getCommonHeaders() const { return getAuthorizationHeader() + acceptHeader; }
    String getJSONHeaders() const { return getCommonHeaders() + contentTypeJSONHeader; }

private:
    String getAuthorizationHeader() const
    {
        String accessToken;

        if (sharedTokens->savedTokens.contains(provider))
        {
            accessToken = sharedTokens->savedTokens[provider];
        }

        return getAuthorizationHeader(accessToken);
    }

    String getAuthorizationHeader(String accessToken) const
    {
        String authorizationHeader;

        if (! accessToken.isEmpty())
        {
            authorizationHeader = "Authorization: Bearer " + accessToken + "\r\n";
        }

        return authorizationHeader;
    }

    SharedResourcePointer<SharedAPIKeys> sharedTokens;

    // Owned by the Model this client belongs to; null for clients that are not
    // tied to a model, in which case requests are simply not abortable.
    RequestRegistry* requestRegistry = nullptr;
};
