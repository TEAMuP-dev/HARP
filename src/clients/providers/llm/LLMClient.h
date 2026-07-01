/**
 * @file LLMClient.h
 * @brief Key-store clients for the LLM providers used by the Model Agent.
 *
 * These providers (Gemini, Anthropic, OpenAI) are not HARP model endpoints --
 * HARP never sends audio to them. They exist so the user can store and validate
 * an LLM API key under the "API Keys" settings tab, which the Model Agent then
 * reads when drafting recipes. Accordingly, the model-processing half of the
 * Client interface is stubbed out; only token registration + validation matter.
 */

#pragma once

#include "../../Client.h"

using namespace juce;

class LLMClient : public Client
{
public:
    // --- Model-processing interface: unused for LLM key-store providers. ---

    String inferHostSlashModel(String) override { return {}; }
    String inferEndpointPath(String) override { return {}; }
    String inferDocumentationPath(String) override { return {}; }

    OpResult queryControls(String, DynamicObject::Ptr&) override
    {
        return OpResult::fail(ClientError { ClientError::Type::UnknownClient, {}, displayName });
    }

    var wrapPayloadElement(var payloadElement, bool = false, String = "") override
    {
        return payloadElement;
    }

    OpResult process(String, String&, std::vector<File>&, LabelList&) override
    {
        return OpResult::fail(ClientError { ClientError::Type::UnknownClient, {}, displayName });
    }

protected:
    String displayName;

    /** GET a URL with explicit headers; succeeds iff the server returns HTTP 200. */
    OpResult httpGetOk(const URL& url, const String& headers, const int timeoutMs = 10000)
    {
        const String path = url.toString(true);

        if (! url.isWellFormed())
        {
            return OpResult::fail(
                HttpError { HttpError::Type::InvalidURL, HttpError::Request::GET, path });
        }

        int statusCode = 0;

        auto options = URL::InputStreamOptions(URL::ParameterHandling::inAddress)
                           .withExtraHeaders(headers)
                           .withConnectionTimeoutMs(timeoutMs)
                           .withStatusCode(&statusCode);

        std::unique_ptr<InputStream> stream(url.createInputStream(options));

        if (stream == nullptr)
        {
            return OpResult::fail(
                HttpError { HttpError::Type::ConnectionFailed, HttpError::Request::GET, path });
        }

        // Drain the body so the connection closes cleanly.
        stream->readEntireStreamAsString();

        if (statusCode != 200)
        {
            return OpResult::fail(
                HttpError { HttpError::Type::BadStatusCode, HttpError::Request::GET, path, statusCode });
        }

        return OpResult::ok();
    }
};

class GeminiClient : public LLMClient
{
public:
    GeminiClient()
    {
        provider = Provider::Gemini;
        displayName = "Google Gemini";
        tokenRegistrationURL = URL("https://aistudio.google.com/app/apikey");
    }

    // Gemini authenticates with the key as a query parameter, not a Bearer header.
    OpResult validateToken(const String& tokenToValidate) override
    {
        URL url = URL("https://generativelanguage.googleapis.com/v1beta/models")
                      .withParameter("key", tokenToValidate);

        return httpGetOk(url, {});
    }
};

class AnthropicClient : public LLMClient
{
public:
    AnthropicClient()
    {
        provider = Provider::Anthropic;
        displayName = "Anthropic Claude";
        tokenRegistrationURL = URL("https://console.anthropic.com/settings/keys");
    }

    // Anthropic uses an x-api-key header and requires a version header.
    OpResult validateToken(const String& tokenToValidate) override
    {
        const String headers =
            "x-api-key: " + tokenToValidate + "\r\n" + "anthropic-version: 2023-06-01\r\n";

        return httpGetOk(URL("https://api.anthropic.com/v1/models"), headers);
    }
};

class OpenAIClient : public LLMClient
{
public:
    OpenAIClient()
    {
        provider = Provider::OpenAI;
        displayName = "OpenAI";
        tokenRegistrationURL = URL("https://platform.openai.com/api-keys");
    }

    // OpenAI uses a Bearer token.
    OpResult validateToken(const String& tokenToValidate) override
    {
        const String headers = "Authorization: Bearer " + tokenToValidate + "\r\n";

        return httpGetOk(URL("https://api.openai.com/v1/models"), headers);
    }
};
