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

    // Optional diagnostic text extracted from the response body
    String detail = {};
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

            userMessage += " request.";

            if (e.request == HttpError::Request::POST)
            {
                userMessage +=
                    "\n\nThe server may be sleeping or temporarily unavailable."
                    "\n\nTry loading again in a few seconds, or click 'Open URL' to view the model's page.";
            }

            return userMessage;

        case HttpError::Type::BadStatusCode:

            if (e.statusCode == 429)
            {
                userMessage = "The service is rate-limiting requests from this machine.";

                if (e.detail.isNotEmpty())
                {
                    userMessage += "\n\nDetails: " + e.detail;
                }

                userMessage += "\n\nThis happens when requests are made in quick succession, "
                               "such as starting and canceling repeatedly. Wait a few moments "
                               "before trying again.";

                return userMessage;
            }

            if (e.statusCode == 402)
            {
                userMessage = "This request could not be completed because the service "
                              "reported payment/quota limits. Please check your account "
                              "usage/billing and try again.";

                if (e.detail.isNotEmpty())
                {
                    userMessage += "\n\nDetails: " + e.detail;
                }

                return userMessage;
            }

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

            userMessage += " request failed";

            if (e.statusCode != 0)
            {
                userMessage += " with status code " + String(e.statusCode);
            }

            userMessage += ".";

            if (e.detail.isNotEmpty())
            {
                userMessage += "\n\nDetails: " + e.detail;
            }

            if (e.statusCode == 503)
            {
                userMessage +=
                    "\n\nIf this is a valid Hugging Face Space, this could indicate "
                    "the Space is paused or down due to a build or runtime error.";
            }
    }

    return userMessage;
}

struct GradioError
{
    enum class Type
    {
        RuntimeError,
        QuotaExceeded,
        Indeterminate,
        IncompleteResponse,
        SpaceStarting,
        SpaceUnavailable
    };

    Type type;

    String endpointPath;
    String reason;
};

inline String toUserMessage(const GradioError& e)
{
    String userMessage = "A Gradio error occurred.";

    switch (e.type)
    {
        case GradioError::Type::RuntimeError:

            userMessage = "The Gradio server reported a runtime error.";

            if (e.reason.isNotEmpty())
            {
                userMessage += "\n\nDetails: " + e.reason;
            }
            else
            {
                userMessage += "\n\nNo details were reported. A Space only forwards the text "
                               "of an error when it is launched with \"show_error=True\" or "
                               "raises a \"gr.Error\". Otherwise the cause appears only in "
                               "its logs, which only the Space's owner can read.";
            }

            userMessage += "\n\nClick 'Open URL' to check the Space's status on Hugging Face.";

            return userMessage;

        case GradioError::Type::QuotaExceeded:

            userMessage = "GPU quota has been exceeded for this Space.";

            if (e.reason.isNotEmpty())
            {
                userMessage += "\n\nDetails: " + e.reason;
            }

            userMessage += "\n\nPlease try again later, or use an account with available quota.";

            return userMessage;

        case GradioError::Type::SpaceStarting:

            userMessage = "The Space is still starting up.";

            if (e.reason.isNotEmpty())
            {
                userMessage += "\n\nIts current state is \"" + e.reason + "\".";
            }

            userMessage += "\n\nSpaces are suspended when idle and take a short while to wake. "
                           "Try loading it again in a minute.";

            return userMessage;

        case GradioError::Type::SpaceUnavailable:

            userMessage = "The Space is not currently available.";

            if (e.reason.isNotEmpty())
            {
                userMessage += "\n\nIts current state is \"" + e.reason + "\".";
            }

            userMessage += "\n\nThis usually means it has been paused by its owner, or has "
                           "stopped because of a build or runtime error. Click 'Open URL' to "
                           "check its status on Hugging Face.";

            return userMessage;

        case GradioError::Type::IncompleteResponse:

            userMessage = "Lost contact with the Space before it returned a result.";

            userMessage +=
                "\n\nThe connection is dropped when nothing arrives for a prolonged period, "
                "which usually means the model was still running. The job may well have "
                "finished on the Space afterwards, but HARP was no longer listening for it.";

            userMessage +=
                "\n\nTry again with a shorter input, or on a Space with faster hardware. "
                "Models that routinely take this long are better suited to a dedicated GPU "
                "Space than to a free CPU one.";

            return userMessage;

        case GradioError::Type::Indeterminate:

            userMessage = "The Space reported an error, but gave no indication of its cause.";

            userMessage +=
                "\n\nThis Space runs on ZeroGPU, so the GPU quota may have been exhausted, "
                "but it could equally be a runtime error in the model. Spaces using a version "
                "of Gradio before 6.13 do not report enough for HARP to tell the two apart.";

            userMessage +=
                "\n\nRunning the model from the Space's own page will report a quota message "
                "if that is the cause, and nothing further if it is not. Click 'Open URL' to "
                "open it on Hugging Face.";

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
        WriteFailed,
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

        case FileError::Type::WriteFailed:

            userMessage = "Failed to write file";

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
        /* Both of these messages send the user to the Space page, since that is
           where the underlying error is actually reported */
        bool pathIsUseful = e->type == GradioError::Type::RuntimeError
                            || e->type == GradioError::Type::Indeterminate
                            || e->type == GradioError::Type::IncompleteResponse
                            || e->type == GradioError::Type::SpaceStarting
                            || e->type == GradioError::Type::SpaceUnavailable;

        if (pathIsUseful && e->endpointPath.isNotEmpty())
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
