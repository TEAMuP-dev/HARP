/**
 * @file
 * @brief Base class for any models that utilize a web api to process
 * information. Currently we provide an implmentation of a wave 2 wave web based
 * model. We use the GradioClient class to communicate with a gradio server.
 * @author hugo flores garcia, aldo aguilar, xribene
 */

#pragma once

#include "HarpLogger.h"
#include "Model.h"
#include "client/Client.h"
#include "client/GradioClient.h"
#include "client/StabilityClient.h"
#include "juce_core/juce_core.h"
#include "utils.h"
#include <fstream>

class WebModel : public Model
{
public:
    WebModel() { status2 = ModelStatus::INITIALIZED; }

    ~WebModel() {}

    bool ready() const override { return m_loaded; }

    ComponentInfoList& getControlsInfo() { return controlsInfo; }
    ComponentInfoList& getInputTracksInfo() { return inputTracksInfo; }
    ComponentInfoList& getOutputTracksInfo() { return outputTracksInfo; }

    // A getter function that gets a Uuid and returns the corresponding info object
    // from any of the three maps
    // This is when using ComponentInfoList
    // Not as efficient as the map version, but it's easier to use vector<pair<>> in the GUI
    std::shared_ptr<PyHarpComponentInfo> findComponentInfoByUuid(const juce::Uuid& id) const
    {
        for (const auto& pair : controlsInfo)
        {
            if (pair.first == id)
                return pair.second;
        }
        for (const auto& pair : inputTracksInfo)
        {
            if (pair.first == id)
                return pair.second;
        }
        for (const auto& pair : outputTracksInfo)
        {
            if (pair.first == id)
                return pair.second;
        }
        return nullptr;
    }

    OpResult parseSpaceAddress(juce::String spaceAddress, SpaceInfo& spaceInfo)
    {
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
        // Create an Error object in case we need it
        Error error;
        // All errors in this function are of type InvalidURL
        error.type = ErrorType::InvalidURL;

        spaceInfo.userInput = spaceAddress;
        juce::String user;
        juce::String model;
        // Check if the URL is of Type 4 (localhost or gradio.live)
        if (spaceAddress.contains("localhost") || spaceAddress.contains("gradio.live")
            || spaceAddress.matchesWildcard("*.*.*.*:*", true))
        {
            spaceInfo.gradio = spaceAddress;
            spaceInfo.apiEndpointURL = spaceAddress;
            spaceInfo.status = SpaceInfo::Status::LOCALHOST;
            spaceInfo.userName = "localhost";
            spaceInfo.modelName = "localhost";
        }
        // 2. Stability AI: "stability/service_type"
        else if (spaceAddress.startsWith("stability/"))
        {
            auto parts = juce::StringArray::fromTokens(spaceAddress, "/", "");
            if (parts.size() == 2)
            {
                user = parts[0]; // "stability"
                model = parts[1]; // serviceType e.g., "text-to-audio"
                spaceInfo.status = SpaceInfo::Status::STABILITY;
                spaceInfo.userName = user;
                spaceInfo.modelName = model;
                spaceInfo.stabilityServiceType = model;

                if (model.equalsIgnoreCase("text-to-audio"))
                {
                    spaceInfo.stability =
                        "https://platform.stability.ai/docs/api-reference#tag/Stable-Audio-2/paths/~1v2beta~1audio~1stable-audio-2~1text-to-audio/post";
                    spaceInfo.apiEndpointURL =
                        "https://api.stability.ai/v2beta/audio/stable-audio-2/text-to-audio";
                }
                else if (model.equalsIgnoreCase("audio-to-audio"))
                {
                    spaceInfo.stability =
                        "https://platform.stability.ai/docs/api-reference#tag/Stable-Audio-2/paths/~1v2beta~1audio~1stable-audio-2~1audio-to-audio/post";
                    spaceInfo.apiEndpointURL =
                        "https://api.stability.ai/v2beta/audio/stable-audio-2/audio-to-audio";
                }
                else
                {
                    spaceInfo.error = "Unsupported Stability AI service type: " + model;
                    spaceInfo.status = SpaceInfo::Status::FAILED;
                    error.devMessage = spaceInfo.error;
                    return OpResult::fail(error);
                }
            }
            else
            {
                spaceInfo.error =
                    "Invalid Stability AI format. Expected 'stability/service_type'. Got: "
                    + spaceAddress;
                spaceInfo.status = SpaceInfo::Status::FAILED;
                error.devMessage = spaceInfo.error;
                return OpResult::fail(error);
            }
        }
        else if (spaceAddress.contains("https://huggingface.co/spaces/"))
        {
            auto spacePath =
                spaceAddress.fromFirstOccurrenceOf("https://huggingface.co/spaces/", false, false);
            auto parts = juce::StringArray::fromTokens(spacePath, "/", "");

            if (parts.size() >= 2)
            {
                user = parts[0];
                model = parts[1];
                spaceInfo.status = SpaceInfo::Status::HUGGINGFACE;
                spaceInfo.huggingface = spaceAddress; // The full HF space URL
                // For Gradio apps hosted on HF, the Gradio API endpoint is usually the hf.space URL
                spaceInfo.gradio = "https://" + user + "-" + model.replace("_", "-") + ".hf.space/";
                spaceInfo.apiEndpointURL = spaceInfo.gradio; // Assuming Gradio client uses this
                spaceInfo.userName = user;
                spaceInfo.modelName = model;
            }
            else
            {
                // result.huggingface = spaceAddress;
                spaceInfo.error = "Detected huggingface.co URL but could not parse user and "
                                  "model. Too few parts in "
                                  + spaceAddress;
                spaceInfo.status = SpaceInfo::Status::FAILED;
                error.devMessage = spaceInfo.error;
                return OpResult::fail(error);
            }
        }
        else if (spaceAddress.contains("hf.space"))
        {
            spaceInfo.apiEndpointURL = spaceAddress;
            // Remove the protocol part (e.g., "https://")
            auto withoutProtocol = spaceAddress.fromFirstOccurrenceOf("://", false, false);

            // Extract the subdomain part before ".hf.space/"
            auto subdomain = withoutProtocol.upToFirstOccurrenceOf(".hf.space", false, false);

            // Split the subdomain at the first hyphen
            auto firstHyphenIndex = subdomain.indexOfChar('-');
            if (firstHyphenIndex != -1)
            {
                user = subdomain.substring(0, firstHyphenIndex);
                model = subdomain.substring(firstHyphenIndex + 1);
                spaceInfo.status = SpaceInfo::Status::GRADIO;
                spaceInfo.userName = user;
                spaceInfo.modelName = model.replace("-", "_");
                spaceInfo.gradio = spaceAddress;

                spaceInfo.huggingface =
                    "https://huggingface.co/spaces/" + user + "/" + spaceInfo.modelName;
            }
            else
            {
                // DBG("No hyphen found in the subdomain." << subdomain);
                // Even though the spaceAddress is supposed to be a gradio URL, we
                // return it as the huggingface URL because result.huggingface is
                // used for the  "Open Space URL" button in the error dialog box
                // result.huggingface = spaceAddress;
                spaceInfo.error = "Detected hf.space URL but could not parse user and model. No "
                                  "hyphen found in the subdomain: "
                                  + subdomain;
                spaceInfo.status = SpaceInfo::Status::FAILED;
                error.devMessage = spaceInfo.error;
                return OpResult::fail(error);
            }
        }
        // else if address is of the form user/model and doesn't contain http
        else if (spaceAddress.contains("/") && ! spaceAddress.contains("http"))
        {
            // Extract user and model
            auto parts = juce::StringArray::fromTokens(spaceAddress, "/", "");
            if (parts.size() == 2)
            {
                user = parts[0];
                model = parts[1];
                // Defaulting to HuggingFace for shorthand user/model if not 'stability'
                spaceInfo.status = SpaceInfo::Status::HUGGINGFACE;
                spaceInfo.huggingface = "https://huggingface.co/spaces/" + user + "/" + model;
                spaceInfo.gradio = "https://" + user + "-" + model.replace("_", "-") + ".hf.space/";
                spaceInfo.apiEndpointURL = spaceInfo.gradio; // Assuming Gradio client
                spaceInfo.userName = user;
                spaceInfo.modelName = model;
                // }
            }
            else
            {
                spaceInfo.error = "Detected user/model URL but could not parse user and model. "
                                  "Too many/few slashes in "
                                  + spaceAddress;
                spaceInfo.status = SpaceInfo::Status::FAILED;
                error.devMessage = spaceInfo.error;
                return OpResult::fail(error);
            }
        }
        else
        {
            spaceInfo.error = "Invalid URL: " + spaceAddress
                              + ". URL does not match any of the expected patterns.";
            spaceInfo.status = SpaceInfo::Status::FAILED;
            error.devMessage = spaceInfo.error;
            return OpResult::fail(error);
        }

        LogAndDBG(spaceInfo.toString());
        return OpResult::ok();
    }

    OpResult load(const map<string, any>& params) override
    {
        // Create an Error object in case we need it
        // and a successful result
        Error error;
        error.type = ErrorType::JsonParseError;
        OpResult result = OpResult::ok();

        status2 = ModelStatus::LOADING;

        std::string userSpaceAddress = std::any_cast<std::string>(params.at("url"));

        SpaceInfo spaceInfo;
        result = parseSpaceAddress(userSpaceAddress, spaceInfo);
        if (result.failed())
        {
            status2 = ModelStatus::ERROR;
            return result;
        }

        //std::unique_ptr<Client> newClient;

        if (spaceInfo.status == SpaceInfo::Status::STABILITY)
        {
            tempClient = std::make_unique<StabilityClient>();
            isStabilityModel = true;
        }
        else
        { // GRADIO, HUGGINFACE, LOCALHOST
            tempClient = std::make_unique<GradioClient>();
            isStabilityModel = false;
        }

        tempClient->setSpaceInfo(spaceInfo);

        LogAndDBG(tempClient->getSpaceInfo().toString());

        // The input components defined in PyHARP include
        // both the input tracks (audio or midi) and the controls (sliders, text boxes, etc)
        juce::Array<juce::var> inputPyharpComponents;
        // The output components only include the output tracks (audio or midi)
        juce::Array<juce::var> outputPyharpComponents;

        juce::DynamicObject cardDict;
        status2 = ModelStatus::GETTING_CONTROLS;
        result = tempClient->getControls(inputPyharpComponents, outputPyharpComponents, cardDict);
        if (result.failed())
        {
            status2 = ModelStatus::ERROR;
            return result;
        }

        // TODO: probably need to check if these properties exist and if they're the right types.
        m_card = ModelCard();
        m_card.name = cardDict.getProperty("name").toString().toStdString();
        m_card.description = cardDict.getProperty("description").toString().toStdString();
        m_card.author = cardDict.getProperty("author").toString().toStdString();

        // tags is a list of str
        juce::Array<juce::var>* tags = cardDict.getProperty("tags").getArray();
        if (tags == nullptr)
        {
            status2 = ModelStatus::ERROR;
            error.devMessage = "Failed to load the tags array from JSON. tags is null.";
            return OpResult::fail(error);
        }

        for (int i = 0; i < tags->size(); i++)
        {
            m_card.tags.push_back(tags->getReference(i).toString().toStdString());
        }

        controlsInfo.clear();
        inputTracksInfo.clear();
        outputTracksInfo.clear();
        uuidsInOrder.clear();

        // iterate through the list of inputComponents
        // inputComponents contain both the input controls i,e sliders, textboxes etc
        // as well as the input media tracks (audio or midi)
        for (int i = 0; i < inputPyharpComponents.size(); i++)
        {
            juce::var pyharpComponent = inputPyharpComponents.getReference(i);
            if (! pyharpComponent.isObject())
            {
                status2 = ModelStatus::ERROR;
                error.devMessage = "Failed to load controls from JSON. control is not an object.";
                return OpResult::fail(error);
            }

            try
            {
                // get the control type
                juce::String type = pyharpComponent["type"].toString().toStdString();

                if (type == "audio_track")
                {
                    auto audio_in = std::make_shared<AudioTrackInfo>();
                    audio_in->id = juce::Uuid();
                    audio_in->label = pyharpComponent["label"].toString().toStdString();
                    audio_in->info = pyharpComponent["info"].toString().toStdString();
                    audio_in->required = stringToBool(pyharpComponent["required"].toString());
                    inputTracksInfo.push_back({ audio_in->id, audio_in });
                    uuidsInOrder.push_back(audio_in->id);
                    LogAndDBG("Audio In: " + audio_in->label + " added");
                }
                else if (type == "midi_track")
                {
                    auto midi_in = std::make_shared<MidiTrackInfo>();
                    midi_in->id = juce::Uuid();
                    midi_in->label = pyharpComponent["label"].toString().toStdString();
                    midi_in->info = pyharpComponent["info"].toString().toStdString();
                    midi_in->required = stringToBool(pyharpComponent["required"].toString());
                    inputTracksInfo.push_back({ midi_in->id, midi_in });
                    uuidsInOrder.push_back(midi_in->id);
                    LogAndDBG("MIDI In: " + midi_in->label + " added");
                }
                else if (type == "slider")
                {
                    auto slider = std::make_shared<SliderInfo>();
                    slider->id = juce::Uuid();
                    slider->label = pyharpComponent["label"].toString().toStdString();
                    slider->info = pyharpComponent["info"].toString().toStdString();
                    slider->minimum = pyharpComponent["minimum"].toString().getFloatValue();
                    slider->maximum = pyharpComponent["maximum"].toString().getFloatValue();
                    slider->step = pyharpComponent["step"].toString().getFloatValue();
                    slider->value = pyharpComponent["value"].toString().getFloatValue();

                    controlsInfo.push_back({ slider->id, slider });
                    uuidsInOrder.push_back(slider->id);
                    LogAndDBG("Slider: " + slider->label + " added");
                }
                else if (type == "text_box")
                {
                    auto text = std::make_shared<TextBoxInfo>();
                    text->id = juce::Uuid();
                    text->label = pyharpComponent["label"].toString().toStdString();
                    text->info = pyharpComponent["info"].toString().toStdString();
                    text->value = pyharpComponent["value"].toString().toStdString();

                    controlsInfo.push_back({ text->id, text });
                    uuidsInOrder.push_back(text->id);
                    LogAndDBG("Text: " + text->label + " added");
                }
                else if (type == "number_box")
                {
                    auto number_box = std::make_shared<NumberBoxInfo>();
                    number_box->id = juce::Uuid();
                    number_box->label = pyharpComponent["label"].toString().toStdString();
                    number_box->info = pyharpComponent["info"].toString().toStdString();
                    number_box->min = pyharpComponent["min"].toString().getFloatValue();
                    number_box->max = pyharpComponent["max"].toString().getFloatValue();
                    number_box->value = pyharpComponent["value"].toString().getFloatValue();

                    controlsInfo.push_back({ number_box->id, number_box });
                    uuidsInOrder.push_back(number_box->id);
                    LogAndDBG("Number Box: " + number_box->label + " added");
                }
                else if (type == "toggle")
                {
                    auto toggle = std::make_shared<ToggleInfo>();
                    toggle->id = juce::Uuid();
                    toggle->label = pyharpComponent["label"].toString().toStdString();
                    toggle->info = pyharpComponent["info"].toString().toStdString();
                    toggle->value = ("1" == pyharpComponent["value"].toString().toStdString());
                    controlsInfo.push_back({ toggle->id, toggle });
                    uuidsInOrder.push_back(toggle->id);
                    LogAndDBG("Toggle: " + toggle->label + " added");
                }
                else if (type == "dropdown")
                {
                    auto dropdown = std::make_shared<ComboBoxInfo>();
                    dropdown->id = juce::Uuid();
                    dropdown->label = pyharpComponent["label"].toString().toStdString();
                    dropdown->info = pyharpComponent["info"].toString().toStdString();
                    juce::Array<juce::var>* choices = pyharpComponent["choices"].getArray();
                    if (choices == nullptr)
                    {
                        status2 = ModelStatus::ERROR;
                        error.devMessage = "Failed to load controls from JSON. options is null.";
                        return OpResult::fail(error);
                    }
                    for (int j = 0; j < choices->size(); j++)
                    {
                        dropdown->options.push_back(choices->getReference(j)
                                                        .getArray()
                                                        ->getFirst()
                                                        .toString()
                                                        .toStdString());
                    }
                    // Check if options is empty
                    if (dropdown->options.empty())
                    {
                        // Don't fail here, just log a warning
                        LogAndDBG("Dropdown control has no options.");
                    }
                    else
                    {
                        // Check if "value" is set
                        if (! pyharpComponent.hasProperty("value"))
                        {
                            // If not, set the value to the first option
                            dropdown->value = dropdown->options[0];
                        }
                        else
                        {
                            dropdown->value = pyharpComponent["value"].toString().toStdString();
                        }
                        controlsInfo.push_back({ dropdown->id, dropdown });
                        // controlsInfo[dropdown->id] = dropdown;
                        uuidsInOrder.push_back(dropdown->id);
                    }
                }
                else
                    LogAndDBG("failed to parse control with unknown type: " + type);
            }
            catch (const char* e)
            {
                status2 = ModelStatus::ERROR;
                error.devMessage = "Failed to load controls from JSON. " + std::string(e);
                return OpResult::fail(error);
            }
        }

        // same for the output pyharp components
        for (int i = 0; i < outputPyharpComponents.size(); i++)
        {
            juce::var pyharpComponent = outputPyharpComponents.getReference(i);
            if (! pyharpComponent.isObject())
            {
                status2 = ModelStatus::ERROR;
                error.devMessage = "Failed to load controls from JSON. control is not an object.";
                return OpResult::fail(error);
            }

            try
            {
                // get the control type
                juce::String type = pyharpComponent["type"].toString().toStdString();

                if (type == "audio_track")
                {
                    auto audio_out = std::make_shared<AudioTrackInfo>();
                    audio_out->id = juce::Uuid();
                    audio_out->label = pyharpComponent["label"].toString().toStdString();
                    audio_out->info = pyharpComponent["info"].toString().toStdString();

                    outputTracksInfo.push_back({ audio_out->id, audio_out });
                    LogAndDBG("Audio Out: " + audio_out->label + " added");
                }
                else if (type == "midi_track")
                {
                    auto midi_out = std::make_shared<MidiTrackInfo>();
                    midi_out->id = juce::Uuid();
                    midi_out->label = pyharpComponent["label"].toString().toStdString();
                    midi_out->info = pyharpComponent["info"].toString().toStdString();

                    outputTracksInfo.push_back({ midi_out->id, midi_out });
                    LogAndDBG("MIDI Out: " + midi_out->label + " added");
                }
            }
            catch (const char* e)
            {
                status2 = ModelStatus::ERROR;
                error.devMessage = "Failed to load controls from JSON. " + std::string(e);
                return OpResult::fail(error);
            }
        }
        loadedClient = std::move(tempClient);
        status2 = ModelStatus::LOADED;
        m_loaded = true;
        return OpResult::ok();
    }

    // The input is a vector of String:File objects corresponding to
    // the files currently loaded in each inputMediaDisplay
    OpResult process(std::vector<std::tuple<Uuid, String, File>> localInputTrackFiles)
    {
        status2 = ModelStatus::STARTING;
        // Create an Error object in case we need it
        // and a successful result
        Error error;
        error.type = ErrorType::JsonParseError;
        OpResult result = OpResult::ok();

        status2 = ModelStatus::SENDING;

        // Clear the outputFilePaths and the labels
        // They will be populated with the new processing results
        outputFilePaths.clear();
        labels.clear();

        // We need to upload all the localInputTrackFiles to the gradio server
        // and get the vector of the remote (uploaded) file paths
        // iterate over the localInputTrackFiles map
        // and upload each file
        // and get the corresponding remote file path
        // juce::StringArray remoteTrackFilePaths;
        // std::map<juce::Uuid, std::string> remoteTrackFilePaths;
        for (auto& tuple : localInputTrackFiles)
        {
            juce::String remoteTrackFilePath;
            result = loadedClient->uploadFileRequest(std::get<2>(tuple), remoteTrackFilePath);
            if (result.failed())
            {
                result.getError().userMessage = "Failed to upload file for track "
                                                + std::get<1>(tuple) + ": "
                                                + std::get<2>(tuple).getFileName();
                status2 = ModelStatus::ERROR;
                return result;
            }
            // remoteTrackFilePaths[std::get<0>(tuple)] = remoteTrackFilePath.toStdString();
            // The following line would be a better way to do it, instead of using the remoteTrackFilePaths dict
            // it won't work though because the pyharpCOmponentInfo in the inputTracksInfo
            // needs to dynamically casted to the correct type AudioTrackInfo or MidiTrackInfo
            // inputTracksInfo[std::get<0>(tuple)]->value = remoteTrackFilePath.toStdString();
            // Here is how to do it:
            auto trackInfo = findComponentInfoByUuid(std::get<0>(tuple));
            if (trackInfo == nullptr)
            {
                status2 = ModelStatus::ERROR;
                error.devMessage = "Failed to upload file for track " + std::get<1>(tuple) + ": "
                                   + std::get<2>(tuple).getFileName()
                                   + ". The track is not an audio or midi track.";
                return OpResult::fail(error);
            }
            if (auto audioTrackInfo = dynamic_cast<AudioTrackInfo*>(trackInfo.get()))
            {
                audioTrackInfo->value = remoteTrackFilePath.toStdString();
            }
            else if (auto midiTrackInfo = dynamic_cast<MidiTrackInfo*>(trackInfo.get()))
            {
                midiTrackInfo->value = remoteTrackFilePath.toStdString();
            }
        }

        // the jsonBody is created by controlsToJson
        juce::String processingPayload;
        result = prepareProcessingPayload(processingPayload);
        if (result.failed())
        {
            result.getError().devMessage = "Failed to upload file";
            status2 = ModelStatus::ERROR;
            return result;
        }

        status2 = ModelStatus::PROCESSING;
        result = loadedClient->processRequest(error, processingPayload, outputFilePaths, labels);
        if (result.failed())
        {
            status2 = ModelStatus::ERROR;
        }
        // Finished status will be set by the MainComponent.h
        // status2 = ModelStatus::FINISHED;
        return result;
    }

    OpResult cancel()
    {
        // Create a successful result.
        // we'll update it to a failure result if something goes wrong
        status2 = ModelStatus::CANCELLING;
        OpResult result = loadedClient->cancel();
        if (result.failed())
        {
            status2 = ModelStatus::ERROR;
            return result;
        }
        status2 = ModelStatus::CANCELLED;
        return result;
    }

    ModelStatus getStatus() { return status2; }

    void setStatus(ModelStatus status) { status2 = status; }

    ModelStatus getLastStatus() { return lastStatus; }
    void setLastStatus(ModelStatus status) { lastStatus = status; }

    Client& getClient() { return *loadedClient; }
    Client& getTempClient() { return *tempClient; }
    // StabilityClient& getStabilityClient() { return stabilityClient; }

    LabelList& getLabels() { return labels; }

    std::vector<juce::String>& getOutputFilePaths() { return outputFilePaths; }

    void clearOutputFilePaths() { outputFilePaths.clear(); }

private:
    OpResult prepareProcessingPayload(juce::String& payloadJson)
    {
        // Create a JSON array to hold each control's value
        juce::Array<juce::var> jsonControlsArray;

        // Iterate through each control in controlsInfo
        // for (const auto& controlPair : controlsInfo)
        for (const auto& currentUuid : uuidsInOrder)
        {
            auto element = findComponentInfoByUuid(currentUuid);
            if (! element)
            {
                // Control not found, handle the error
                Error error;
                // error.type = ErrorType::MissingJsonKey;
                error.devMessage =
                    "Control with ID: " + currentUuid.toString() + " not found in controlsInfo.";
                return OpResult::fail(error);
            }

            juce::var controlValue;
            bool isFile = false;

            // Check the type of control and extract its value
            if (auto sliderControl = dynamic_cast<SliderInfo*>(element.get()))
            {
                controlValue = juce::var(sliderControl->value);
            }
            else if (auto textBoxControl = dynamic_cast<TextBoxInfo*>(element.get()))
            {
                controlValue = juce::var(textBoxControl->value);
            }
            else if (auto numberBoxControl = dynamic_cast<NumberBoxInfo*>(element.get()))
            {
                controlValue = juce::var(numberBoxControl->value);
            }
            else if (auto toggleControl = dynamic_cast<ToggleInfo*>(element.get()))
            {
                controlValue = juce::var(toggleControl->value);
            }
            else if (auto comboBoxControl = dynamic_cast<ComboBoxInfo*>(element.get()))
            {
                controlValue = juce::var(comboBoxControl->value);
            }

            // Audio Input
            else if (auto audioInTrackInfo = dynamic_cast<AudioTrackInfo*>(element.get()))
            {
                if (audioInTrackInfo->value.empty())
                {
                    controlValue = juce::var(); // null
                }
                else
                {
                    juce::DynamicObject::Ptr fileObj = new juce::DynamicObject();
                    fileObj->setProperty("path", juce::var(audioInTrackInfo->value));

                    juce::DynamicObject::Ptr meta = new juce::DynamicObject();
                    meta->setProperty("_type", juce::var("gradio.FileData"));
                    fileObj->setProperty("meta", juce::var(meta));

                    controlValue = juce::var(fileObj);
                }

                isFile = true;
            }

            // MIDI Input
            else if (auto midiInTrackInfo = dynamic_cast<MidiTrackInfo*>(element.get()))
            {
                // skip MIDI input for Stability models
                if (isStabilityModel)
                    continue;

                if (midiInTrackInfo->value.empty())
                {
                    controlValue = juce::var(); // null
                }
                else
                {
                    juce::DynamicObject::Ptr fileObj = new juce::DynamicObject();
                    fileObj->setProperty("path", juce::var(midiInTrackInfo->value));

                    juce::DynamicObject::Ptr meta = new juce::DynamicObject();
                    meta->setProperty("_type", juce::var("gradio.FileData"));
                    fileObj->setProperty("meta", juce::var(meta));

                    controlValue = juce::var(fileObj);
                }

                isFile = true;
            }

            else
            {
                Error error;
                error.type = ErrorType::UnsupportedControlType;
                error.devMessage =
                    "Unsupported control type for control with UUID: " + currentUuid.toString();
                return OpResult::fail(error);
            }

            // wrapping for Stability
            if (isStabilityModel)
            {
                juce::DynamicObject::Ptr wrapped = new juce::DynamicObject();

                // Important: use "input" label for audio file — required for Stability AI
                const juce::String label =
                    isFile ? juce::String("input") : juce::String(element->label);

                wrapped->setProperty("label", label);
                wrapped->setProperty("value", controlValue);

                jsonControlsArray.add(juce::var(wrapped));
            }
            else
            {
                // For Gradio: just add raw values
                jsonControlsArray.add(controlValue);
            }
        }

        juce::DynamicObject::Ptr dataObject = new juce::DynamicObject();
        dataObject->setProperty("data", jsonControlsArray);

        payloadJson = juce::JSON::toString(juce::var(dataObject), true);
        DBG("prepareProcessingPayload: " + payloadJson);

        return OpResult::ok();
    }

    bool isStabilityModel =
        false; // A flag to indicate if the current model is a Stability AI model
    ComponentInfoList controlsInfo;
    ComponentInfoList inputTracksInfo;
    ComponentInfoList outputTracksInfo;
    // A vector that stores the Uuid of the input and control components
    // in the order they are received from the server
    // We need to keep track the order to be able to send the data
    // for processing in the same order.
    // We wouldn't have to do that if
    //    1. c++ had an ordered map (like python)
    //    2. the gradio server would accept key:value pairs instead of list
    std::vector<juce::Uuid> uuidsInOrder;
    std::unique_ptr<Client> loadedClient;
    std::unique_ptr<Client> tempClient;
    // GradioClient gradioClient;
    // StabilityClient stabilityClient;

    // A helper variable to store the status of the model
    // before loading a new model. If the new model fails to load,
    // we want to go back to the status we had before the failed attempt
    ModelStatus lastStatus;

    // A variable to store the latest labelList received during processing
    LabelList labels;
    // A vector to store the output file paths we get from gradio
    // after processing. We assume that the order of the output files
    // is the same as the order of the outputTracksInfo
    std::vector<juce::String> outputFilePaths;
};

// a timer that checks the status of the model and broadcasts a change if if there is one
class ModelStatusTimer : public juce::Timer, public juce::ChangeBroadcaster
{
public:
    ModelStatusTimer(std::shared_ptr<WebModel> model) : m_model(model) {}

    void timerCallback() override
    {
        // get the status of the model
        ModelStatus status = m_model->getStatus();
        // DBG("ModelStatusTimer::timerCallback status: " + std::to_string(status)
        //     + " lastStatus: " + std::to_string(lastStatus));

        // if the status has changed, broadcast a change
        if (status != lastStatus)
        {
            lastStatus = status;
            sendChangeMessage();
        }
    }

    void setModel(std::shared_ptr<WebModel> model)
    {
        // stopTimer();
        m_model = model;
        // lastStatus = ModelStatus::INITIALIZED;
        // startTimer(50);
    }

private:
    std::shared_ptr<WebModel> m_model;
    ModelStatus lastStatus;
};
