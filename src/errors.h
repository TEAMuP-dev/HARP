
/**
 * @file
 * @brief classes and functions for handling errors in the application
 * @author xribene
 */

#pragma once

#include <juce_core/juce_core.h>
// #include <juce_core/misc/juce_Result.h>

enum class ErrorType
{
    InvalidURL,
    MissingJsonKey,
    JsonParseError,
    FileUploadError,
    FileDownloadError,
    HttpRequestError,
    UnknownError,
    UnsupportedControlType,
    UnknownLabelType,
};

struct Error
{
    ErrorType type;
    juce::String devMessage;
    juce::String userMessage;
};

/*
* Wrapper class for operation results
* This class wraps JUCE's Result class and adds an Error object
*/

class OpResult
{
public:
    // Create a successful result
    static OpResult ok() noexcept { return OpResult(juce::Result::ok(), {}); }

    // Create a failure result with an Error object
    static OpResult fail(const Error& error) noexcept
    {
        return OpResult(juce::Result::fail(error.devMessage), error);
    }

    // Check if this result indicates success
    bool wasOk() const noexcept { return result.wasOk(); }

    // Check if this result indicates failure
    bool failed() const noexcept { return result.failed(); }

    // Get the Error object (contains developer and user messages)
    Error& getError() noexcept { return error; }

    operator bool() const noexcept
    {
        // True if operation was successful (not failed)
        return ! failed(); 
    }

private:
    juce::Result result; // Use JUCE's Result internally
    Error error; // Custom Error object

    // Private constructor for the wrapper class
    OpResult(const juce::Result& res, const Error& err) noexcept : result(res), error(err) {}
};

// void mapErrorToUserMessage(Error& error)
// {
//     switch (error.type)
//     {
//         case ErrorType::InvalidURL:
//             error.userMessage =
//                 "The URL you entered seems to be invalid. Please check the format and try again.";
//             break;

//         case ErrorType::MissingParts:
//         case ErrorType::UserModelError:
//             error.userMessage =
//                 "The URL is incomplete. Please enter the full address and try again.";
//             break;

//         case ErrorType::NoHyphen:
//             error.userMessage =
//                 "There seems to be an issue with the URL. Please verify it and try again.";
//             break;

//         case ErrorType::UnknownError:
//         default:
//             error.userMessage =
//                 "An unknown error occurred while processing the URL. Please verify it and try again.";
//             break;
//     }
// }