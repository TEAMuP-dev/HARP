/**
 * @file Errors.h
 * @brief Classes and helper functions for error handling.
 * @author xribene, cwitkowitz
 */

#pragma once

#include <juce_core/juce_core.h>

using namespace juce;

struct ClientError
{
    enum class Type
    {
        UnknownClient,
        InvalidModelPath,
        InsufficientPermissions
    };

    Type type;

    String path;
    String client;
    String token;
};

inline String toUserMessage(const ClientError& e)
{
    String userMessage = "A client error occurred.";

    switch (e.type)
    {
        case ClientError::Type::UnknownClient:

            userMessage = "Path ";

            if (e.path.isNotEmpty())
            {
                userMessage += "\"" + e.path + "\" ";
            }

            userMessage += "does not match valid specification for any supported clients.";

            return userMessage;

        case ClientError::Type::InvalidModelPath:

            userMessage = "Path ";

            if (e.path.isNotEmpty())
            {
                userMessage += "\"" + e.path + "\" ";
            }

            userMessage += "does not match valid specification for client";

            if (e.client.isNotEmpty())
            {
                userMessage += " \"" + e.client + "\"";
            }

            userMessage += ".";

            return userMessage;

        case ClientError::Type::InsufficientPermissions:

            if (e.token.isEmpty())
            {
                userMessage = "Authorization for client";

                if (e.client.isNotEmpty())
                {
                    userMessage += " \"" + e.client + "\"";
                }

                userMessage +=
                    " failed. Please make sure you have added a valid API key in settings.";
            }
            else
            {
                userMessage = "Token \"" + e.token + "\" for client";

                if (e.client.isNotEmpty())
                {
                    userMessage += " \"" + e.client + "\"";
                }

                userMessage += " does not have sufficient permissions.";
            }

            return userMessage;
    }

    return userMessage;
}

struct HttpError
{
    enum class Type
    {
        InvalidURL,
        ConnectionFailed,
        BadStatusCode,
        //InvalidResponse
    };

    Type type;

    enum class Request
    {
        POST,
        GET
    };

    Request request;

    String endpointPath;

    int statusCode = 0;
};

inline String toUserMessage(const HttpError& e)
{
    String userMessage = "An HTTP error occurred.";

    switch (e.type)
    {
        case HttpError::Type::InvalidURL:

            userMessage = "Endpoint URL ";

            if (e.endpointPath.isNotEmpty())
            {
                userMessage += "\"" + e.endpointPath + "\" ";
            }

            userMessage += "is malformed.";

            return userMessage;

        case HttpError::Type::ConnectionFailed:

            userMessage = "Unable to make ";

            if (e.request == HttpError::Request::POST)
            {
                userMessage += "POST";
            }
            else if (e.request == HttpError::Request::GET)
            {
                userMessage += "GET";
            }
            else
            {
            }

            userMessage += " request to endpoint";

            if (e.endpointPath.isNotEmpty())
            {
                userMessage += " \"" + e.endpointPath + "\"";
            }

            userMessage += ".";

            if (e.request == HttpError::Request::POST)
            {
                userMessage += " If this is a valid Hugging Face space, this "
                               "could indicate the space is sleeping or restarting. "
                               "Please try again in a few minutes.";
            }

            return userMessage;

        case HttpError::Type::BadStatusCode:

            userMessage.clear();

            if (e.request == HttpError::Request::POST)
            {
                userMessage += "POST";
            }
            else if (e.request == HttpError::Request::GET)
            {
                userMessage += "GET";
            }
            else
            {
            }

            userMessage += " request to endpoint ";

            if (e.endpointPath.isNotEmpty())
            {
                userMessage += "\"" + e.endpointPath + "\" ";
            }

            userMessage += "failed";

            if (e.statusCode != 0)
            {
                userMessage += " with status code " + String(e.statusCode);
            }

            userMessage += ".";

            if (e.statusCode == 503)
            {
                userMessage += " If this is a valid Hugging Face space, this could indicate "
                               "the space is paused or down due to a build or runtime error.";
            }
    }

    return userMessage;
}

struct GradioError
{
    enum class Type
    {
        RuntimeError
    };

    Type type;

    String endpointPath;
};

inline String toUserMessage(const GradioError& e)
{
    String userMessage = "A Gradio error occurred.";

    switch (e.type)
    {
        case GradioError::Type::RuntimeError:

            userMessage = "A runtime error occurred at endpoint";

            if (e.endpointPath.isNotEmpty())
            {
                userMessage += " \"" + e.endpointPath + "\"";
            }

            userMessage += ". If this is a Hugging Face space running on ZeroGPU, this "
                           "can also indicate a user has exceeded their daily ZeroGPU quota.";

            return userMessage;
    }

    return userMessage;
}

struct JsonError
{
    enum class Type
    {
        InvalidJSON,
        NotADictionary,
        NotAnArray,
        Empty,
        MissingKey
    };

    Type type;

    String stringJSON;
    String key;
};

inline String toUserMessage(const JsonError& e)
{
    String userMessage = "A JSON error occurred.";

    switch (e.type)
    {
        case JsonError::Type::InvalidJSON:

            userMessage = "Unable to parse JSON";

            if (e.stringJSON.isNotEmpty())
            {
                userMessage += " \"" + e.stringJSON + "\"";
            }

            userMessage += ".";

            return userMessage;

        case JsonError::Type::NotADictionary:

            userMessage = "JSON ";

            if (e.stringJSON.isNotEmpty())
            {
                userMessage += "\"" + e.stringJSON + "\" ";
            }

            userMessage += "is not a valid dictionary.";

            return userMessage;

        case JsonError::Type::NotAnArray:

            userMessage = "JSON ";

            if (e.stringJSON.isNotEmpty())
            {
                userMessage += "\"" + e.stringJSON + "\" ";
            }

            userMessage += "is not a valid array.";

            return userMessage;

        case JsonError::Type::Empty:

            userMessage = "JSON is empty.";

            return userMessage;

        case JsonError::Type::MissingKey:

            userMessage = "JSON ";

            if (e.stringJSON.isNotEmpty())
            {
                userMessage += "\"" + e.stringJSON + "\" ";
            }

            userMessage += "is missing key";

            if (e.key.isNotEmpty())
            {
                userMessage += " \"" + e.key + "\"";
            }

            userMessage += ".";

            return userMessage;
    }

    return userMessage;
}

struct ControlError
{
    enum class Type
    {
        UnsupportedControl
    };

    Type type;

    String controlType;
};

inline String toUserMessage(const ControlError& e)
{
    String userMessage = "A control error occurred.";

    switch (e.type)
    {
        case ControlError::Type::UnsupportedControl:

            userMessage = "HARP does not currently support controls of type";

            if (e.controlType.isNotEmpty())
            {
                userMessage += " \"" + e.controlType + "\"";
            }

            userMessage += ".";

            return userMessage;
    }

    return userMessage;
}

struct FileError
{
    enum class Type
    {
        DoesNotExist,
        UploadFailed,
        DownloadFailed,
        UnsupportedFormat
    };

    Type type;

    String path;
};

inline String toUserMessage(const FileError& e)
{
    String userMessage = "A file error occurred.";

    switch (e.type)
    {
        case FileError::Type::DoesNotExist:

            userMessage = "File";

            if (e.path.isNotEmpty())
            {
                userMessage += " at path \"" + e.path + "\"";
            }

            userMessage += "does not exist.";

            return userMessage;

        case FileError::Type::UploadFailed:

            userMessage = "Failed to upload file";

            if (e.path.isNotEmpty())
            {
                userMessage += " at path \"" + e.path + "\"";
            }

            userMessage += ".";

            return userMessage;

        case FileError::Type::DownloadFailed:

            userMessage = "Failed to download file";

            if (e.path.isNotEmpty())
            {
                userMessage += " at path \"" + e.path + "\"";
            }

            userMessage += ".";

            return userMessage;

        case FileError::Type::UnsupportedFormat:

            userMessage = "File format";

            if (e.path.isNotEmpty())
            {
                String extension = File::createFileWithoutCheckingPath(e.path).getFileExtension();

                userMessage += " \"" + extension + "\"";
            }

            userMessage += " is not supported.";

            return userMessage;
    }

    return userMessage;
}

using Error = std::variant<ClientError, HttpError, GradioError, JsonError, ControlError, FileError>;

inline String toUserMessage(const Error& error)
{
    return std::visit([](const auto& e) { return toUserMessage(e); }, error);
}

inline std::optional<String> getOpenablePath(const Error& error)
{
    if (const auto* e = std::get_if<HttpError>(&error))
    {
        if (e->type == HttpError::Type::ConnectionFailed && e->endpointPath.isNotEmpty())
        {
            return e->endpointPath;
        }
        else if (e->type == HttpError::Type::BadStatusCode && e->endpointPath.isNotEmpty())
        {
            return e->endpointPath;
        }
    }

    if (const auto* e = std::get_if<GradioError>(&error))
    {
        if (e->type == GradioError::Type::RuntimeError && e->endpointPath.isNotEmpty())
        {
            return e->endpointPath;
        }
    }

    return std::nullopt;
}

class OpResult
{
public:
    /**
     * Create a result indicating success.
     */
    static OpResult ok() noexcept { return OpResult(Result::ok(), std::nullopt); }

    /**
     * Create a result indicating failure with a specific error.
     */
    static OpResult fail(Error error)
    {
        return OpResult(Result::fail(toUserMessage(error)), std::move(error));
    }

    /**
     * Check if this result indicates success.
     */
    bool wasOk() const noexcept { return result.wasOk(); }

    /**
     * Check if this result indicates failure.
     */
    bool failed() const noexcept { return result.failed(); }

    /**
     * Obtain the error associated with this result if one exists.
     */
    const Error& getError() const noexcept
    {
        jassert(error.has_value());
        return *error;
    }

    explicit operator bool() const noexcept { return wasOk(); }

private:
    OpResult(Result r, std::optional<Error> e) : result(std::move(r)), error(std::move(e)) {}

    Result result;

    std::optional<Error> error;
};
