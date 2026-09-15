/**
 * @file Errors.h
 * @brief Classes and helper functions for error handling.
 * @author xribene, cwitkowitz
 */

#pragma once

#include <juce_core/juce_core.h>

#include "Enums.h"

using namespace juce;

struct ClientError
{
    enum class Type
    {
        UnknownClient,
        InvalidModelPath,
        ModelNotFound,
        InsufficientPermissions
    };

    Type type;

    String path;
    String client;
    String token;
};

inline String toUserMessage(const ClientError& e)
{
    String userMessage = "Something went wrong while setting up the model.";

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

        case ClientError::Type::ModelNotFound:

            userMessage = "No model was found at ";

            if (e.path.isNotEmpty())
            {
                userMessage += "\"" + e.path + "\"";
            }
            else
            {
                userMessage += "the given path";
            }

            /* Underscores and hyphens are interchangeable in the address used to
               reach a model but not in its name, so a misspelling of one for the
               other is the likeliest reason to land here. */
            userMessage += ". Please check the spelling, including any underscores"
                           " and capitalization. If the model is private, make sure"
                           " you have added a valid API key in settings.";

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
        UnexpectedResponse
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
    String userMessage = "Something went wrong while contacting the model.";

    switch (e.type)
    {
        case HttpError::Type::InvalidURL:

            if (e.endpointPath.isNotEmpty())
            {
                userMessage = "\"" + e.endpointPath + "\" is not a valid address.";
            }
            else
            {
                userMessage = "The model's address is not valid.";
            }

            return userMessage;

        case HttpError::Type::ConnectionFailed:

            userMessage = "Could not reach the model.";

            userMessage += "\n\nIt may be asleep, starting up, or temporarily unavailable."
                           "\n\nTry again in a few seconds, or click 'Open URL' to view the "
                           "model's page.";

            return userMessage;

        case HttpError::Type::UnexpectedResponse:

            userMessage = "This address did not respond like a model endpoint.";

            userMessage += "\n\nA web page was returned instead of model data. Check that the "
                           "address points at a running Gradio app, and that it does not require "
                           "signing in through a browser.";

            return userMessage;

        case HttpError::Type::BadStatusCode:

            if (e.statusCode == 401 || e.statusCode == 403)
            {
                userMessage = "This model requires authentication.";

                if (e.detail.isNotEmpty())
                {
                    userMessage += "\n\nDetails: " + e.detail;
                }

                userMessage += "\n\nIt may be private or gated. Add an API key for this "
                               "provider under Settings, and make sure the account it belongs "
                               "to has been granted access.";

                return userMessage;
            }

            if (e.statusCode == 404)
            {
                userMessage = "No model was found at this address.";

                userMessage += "\n\nCheck the path for typos. If the model is hosted on "
                               "Hugging Face, confirm the Space still exists and is public.";

                return userMessage;
            }

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

            userMessage = "The server could not complete the request";

            if (e.statusCode != 0)
            {
                userMessage += " (error " + String(e.statusCode) + ")";
            }

            userMessage += ".";

            if (e.detail.isNotEmpty())
            {
                userMessage += "\n\nDetails: " + e.detail;
            }

            if (e.statusCode == 503)
            {
                userMessage += "\n\nThe model may be paused, or down because of a build "
                               "or runtime error.";
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

    /* Whether the endpoint is a Hugging Face Space, so that messages can name it
       accurately instead of guessing. Set by the client, which is the only layer
       that knows how to classify a model path. */
    bool isSpace = false;

    /** "Space" or "model", whichever this endpoint actually is. */
    String describeTarget() const { return isSpace ? "Space" : "model"; }
};

inline String toUserMessage(const GradioError& e)
{
    String userMessage = "The model reported an error.";

    switch (e.type)
    {
        case GradioError::Type::RuntimeError:

            userMessage = "The " + e.describeTarget() + " reported an error while processing.";

            if (e.reason.isNotEmpty())
            {
                userMessage += "\n\nDetails: " + e.reason;
            }
            else
            {
                userMessage += "\n\nNo details were reported. A Gradio app only forwards the "
                               "text of an error when it is launched with \"show_error=True\" "
                               "or raises a \"gr.Error\". Otherwise the cause appears only in "
                               "the server's own logs, which are visible only to whoever runs it.";
            }

            userMessage += "\n\nClick 'Open URL' to open the " + e.describeTarget() + "'s page.";

            return userMessage;

        case GradioError::Type::QuotaExceeded:

            userMessage = "GPU quota has been exceeded for this " + e.describeTarget() + ".";

            if (e.reason.isNotEmpty())
            {
                userMessage += "\n\nDetails: " + e.reason;
            }

            userMessage += "\n\nPlease try again later, or use an account with available quota."
                           "\n\nClick 'Open URL' to open the "
                           + e.describeTarget() + "'s page.";

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

            userMessage =
                "Lost contact with the " + e.describeTarget() + " before it returned a result.";

            userMessage +=
                "\n\nThe connection is dropped when nothing arrives for a prolonged period, "
                "which most often means the model was still working. The job may well have "
                "finished on the server afterwards, but HARP was no longer listening for it.";

            userMessage +=
                "\n\nTry again with a shorter input, or on faster hardware. A model that "
                "routinely takes this long is better suited to a dedicated GPU than to a "
                "free CPU instance.";

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

/**
 * A compact technical description intended for the log file.
 *
 * This is deliberately separate from toUserMessage: the popup explains what to do
 * about a failure, whereas this records which branch produced it, so that a report
 * accompanied by a log can be traced back to a specific decision.
 */
inline String toLogString(const Error& error)
{
    if (const auto* e = std::get_if<ClientError>(&error))
    {
        return "ClientError(" + enumToString(e->type) + ", path=\"" + e->path + "\", client=\""
               + e->client + "\")";
    }

    if (const auto* e = std::get_if<HttpError>(&error))
    {
        return "HttpError(" + enumToString(e->type) + ", " + enumToString(e->request)
               + ", status=" + String(e->statusCode) + ", endpoint=\"" + e->endpointPath
               + "\", detail=\"" + e->detail + "\")";
    }

    if (const auto* e = std::get_if<GradioError>(&error))
    {
        return "GradioError(" + enumToString(e->type) + ", isSpace=" + (e->isSpace ? "1" : "0")
               + ", endpoint=\"" + e->endpointPath + "\", reason=\"" + e->reason + "\")";
    }

    if (const auto* e = std::get_if<JsonError>(&error))
    {
        return "JsonError(" + enumToString(e->type) + ", key=\"" + e->key + "\", json=\""
               + e->stringJSON.substring(0, 400) + "\")";
    }

    if (const auto* e = std::get_if<FileError>(&error))
    {
        return "FileError(" + enumToString(e->type) + ", path=\"" + e->path + "\")";
    }

    if (const auto* e = std::get_if<ControlError>(&error))
    {
        return "ControlError(" + enumToString(e->type) + ")";
    }

    return "UnknownError";
}

inline std::optional<String> getOpenablePath(const Error& error)
{
    if (const auto* e = std::get_if<HttpError>(&error))
    {
        /* A malformed address is the one case with nothing to open, since it is the
           address itself that is at fault. */
        if (e->type != HttpError::Type::InvalidURL && e->endpointPath.isNotEmpty())
        {
            return e->endpointPath;
        }
    }

    if (const auto* e = std::get_if<GradioError>(&error))
    {
        /* Every one of these messages sends the user to the model's own page, since
           that is where the underlying error is actually reported. */
        if (e->endpointPath.isNotEmpty())
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
