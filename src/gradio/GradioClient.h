/**
 * @file
 * @brief A GradioClient class for making http requests to the Gradio API
 * @author  xribene
 */

#pragma once

#include <fstream>

#include "../HarpLogger.h"
#include "../errors.h"
#include "../utils.h"
#include "juce_core/juce_core.h"
class GradioClient

{
public:
    // GradioClient(const juce::String& spaceUrl);
    GradioClient() = default;

    OpResult extractKeyFromResponse(const juce::String& response,
                                    juce::String& responseKey,
                                    const juce::String& key) const;

    OpResult uploadFileRequest(const juce::File& fileToUpload,
                               juce::String& uploadedFilePath,
                               const int timeoutMs = 10000) const;

    OpResult makePostRequestForEventID(const juce::String endpoint,
                                       juce::String& eventId,
                                       const juce::String jsonBody = R"({"data": []})",
                                       const int timeoutMs = 10000) const;

    OpResult getResponseFromEventID(const juce::String callID,
                                    const juce::String eventID,
                                    juce::String& response,
                                    const int timeoutMs = 10000) const;

    OpResult getControls(juce::Array<juce::var>& inputComponents,
                         juce::Array<juce::var>& outputComponents,
                         juce::DynamicObject& cardDict);

    OpResult setSpaceInfo(const juce::String url);

    SpaceInfo getSpaceInfo() const;

    OpResult downloadFileFromURL(const juce::URL& fileURL,
                                 juce::String& downloadedFilePath,
                                 const int timeoutMs = 10000) const;

private:
    static OpResult parseSpaceAddress(juce::String spaceAddress, SpaceInfo& spaceInfo);
    /***
    We parse the space address given by the user
    which can take 4 forms:
        "http://localhost:7860", (gradio app)
        "https://xribene-midi-pitch-shifter.hf.space/", (gradio app)
        "https://huggingface.co/spaces/xribene/midi_pitch_shifter", (hf repo)
        "xribene/midi_pitch_shifter",

    and we store the parsed information in a SpaceInfo object
    e.g
    {
        "huggingface":
    "https://huggingface.co/spaces/xribene/midi_pitch_shifter", "gradio":
    "https://xribene-midi-pitch-shifter.hf.space/", "userInput":
    "xribene/midi_pitch_shifter", "modelName": "midi_pitch_shifter", "userName":
    "xribene"
    }
    ***/
    SpaceInfo spaceInfo;
};