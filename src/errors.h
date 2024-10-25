
/**
 * @file
 * @brief classes and functions for handling errors in the application
 * @author xribene
 */

#pragma once

#include <juce_core/juce_core.h>
// #include <string>
// #include <vector>
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

    /*
    * In this function, we parse the detailed developer message 
    * of an Error object, and fill the user message accordingly.
    * by default, we set the user message to the developer message.
    */
    static void fillUserMessage(Error& error)
    {
        error.userMessage = error.devMessage;
        if (error.devMessage.contains("503"))
        {
            error.userMessage =
                "The gradio app is currently paused by the developer. Please try again later.";
        }
        else if (error.type == ErrorType::HttpRequestError)
        {
            // if (error.devMessage.contains("POST request to controls"))
            if (containsAllSubstrings(error.devMessage, { "POST", "controls", "input stream" }))
            {
                error.userMessage = "TIMEOUT: The gradio app is SLEEPING. Please try again later.";
            }
        }
        else if (error.type == ErrorType::FileUploadError)
        {
            error.userMessage =
                "Failed to upload the file to the gradio app. Please check you internet connection.";
        }
    }

    // Function to check if all substrings are contained in the given string
    static bool containsAllSubstrings(const juce::String& str,
                                      const std::vector<std::string>& substrings)
    {
        for (const auto& substring : substrings)
        {
            if (! str.contains(substring))
            {
                return false;
            }
        }
        return true; // All substrings are found
    }
};
// type: HttpRequestError - Failed to create input stream for POST request to controls
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

// /*
// * In this function, we parse the detailed developer message
// * of an Error object, and fill the user message accordingly.
// * by default, we set the user message to the developer message.
// */
// void fillUserMessage(Error& error)
// {
//     error.userMessage = error.devMessage;
//     if (error.devMessage.contains("503"))
//     {
//         error.userMessage = "The gradio app is currently paused by the developer. Please try again later.";
//     }
// }
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