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
#include "gradio/GradioClient.h"
#include "juce_core/juce_core.h"
#include "utils.h"
#include <fstream>

class WebModel : public Model
{
public:
    WebModel() { status2 = ModelStatus::INITIALIZED; }

    ~WebModel() {}

    bool ready() const override { return status2 == ModelStatus::LOADED; }

    ComponentInfoList& getControlsInfo() { return controlsInfo; }
    ComponentInfoList& getInputTracksInfo() { return inputTracksInfo; }
    ComponentInfoList& getOutputTracksInfo() { return outputTracksInfo; }

    // A getter function that gets a Uuid and returns the corresponding info object
    // from any of the three maps
    // This is when using ComponentInfoMap
    // std::shared_ptr<PyHarpComponentInfo> findComponentInfoByUuid(const juce::Uuid& id) const
    // {
    //     auto pair = controlsInfo.find(id);
    //     if (pair != controlsInfo.end())
    //         return pair->second;

    //     pair = inputTracksInfo.find(id);
    //     if (pair != inputTracksInfo.end())
    //         return pair->second;

    //     pair = outputTracksInfo.find(id);
    //     if (pair != outputTracksInfo.end())
    //         return pair->second;

    //     return nullptr;
    // }

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

    OpResult load(const map<string, any>& params) override
    {
        // Create an Error object in case we need it
        // and a successful result
        Error error;
        error.type = ErrorType::JsonParseError;
        OpResult result = OpResult::ok();

        // controlsInfo.clear();
        status2 = ModelStatus::LOADING;

        // get the name of the huggingface repo we're going to use
        // if (! modelparams::contains(params, "url"))
        // {
        //     error.devMessage = "url key was not found in params while loading the model.";
        //     return OpResult::fail(error);
        // }

        std::string userSpaceAddress = std::any_cast<std::string>(params.at("url"));

        result = gradioClient.setSpaceInfo(userSpaceAddress);

        // if (gradioClient.getSpaceInfo().status == SpaceInfo::Status::FAILED)
        if (result.failed())
        {
            status2 = ModelStatus::ERROR;
            return result;
        }

        LogAndDBG(gradioClient.getSpaceInfo().toString());

        juce::Array<juce::var> inputPyharpComponents;
        juce::Array<juce::var> outputPyharpComponents;

        juce::DynamicObject cardDict;
        status2 = ModelStatus::GETTING_CONTROLS;
        result = gradioClient.getControls(inputPyharpComponents, outputPyharpComponents, cardDict);
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
        // m_card.midi_in = (bool) cardDict.getProperty("midi_in");
        // m_card.midi_out = (bool) cardDict.getProperty("midi_out");

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

        // clear the m_controls vector
        controlsInfo.clear();
        inputTracksInfo.clear();
        outputTracksInfo.clear();

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

                // For the first two, we are abusing the term control.
                // They are actually the main inputs to the model (audio or midi)
                if (type == "audio_track")
                {
                    // CB:TODO: NOT USED ANYWHERE
                    // ControlAreaWidget.h ignores this when populating the GUI
                    auto audio_in = std::make_shared<AudioTrackInfo>();
                    // cb: maybe the id doesn't need to be stored inside the object
                    // since now we are using a map to store the objects
                    audio_in->id = juce::Uuid();
                    audio_in->label = pyharpComponent["label"].toString().toStdString();

                    inputTracksInfo.push_back({ audio_in->id, audio_in });
                    // inputTracksInfo[audio_in->id] = audio_in;
                    uuidsInOrder.push_back(audio_in->id);
                    LogAndDBG("Audio In: " + audio_in->label + " added");
                }
                else if (type == "midi_track")
                {
                    auto midi_in = std::make_shared<MidiTrackInfo>();
                    midi_in->id = juce::Uuid();
                    midi_in->label = pyharpComponent["label"].toString().toStdString();

                    inputTracksInfo.push_back({ midi_in->id, midi_in });
                    // inputTracksInfo[midi_in->id] = midi_in;
                    uuidsInOrder.push_back(midi_in->id);
                    LogAndDBG("MIDI In: " + midi_in->label + " added");
                }
                // The rest are the actual controls that map to hyperparameters
                // of the model
                else if (type == "slider")
                {
                    auto slider = std::make_shared<SliderInfo>();
                    slider->id = juce::Uuid();
                    slider->label = pyharpComponent["label"].toString().toStdString();
                    slider->minimum = pyharpComponent["minimum"].toString().getFloatValue();
                    slider->maximum = pyharpComponent["maximum"].toString().getFloatValue();
                    slider->step = pyharpComponent["step"].toString().getFloatValue();
                    slider->value = pyharpComponent["value"].toString().getFloatValue();

                    controlsInfo.push_back({ slider->id, slider });
                    // controlsInfo[slider->id] = slider;
                    uuidsInOrder.push_back(slider->id);
                    LogAndDBG("Slider: " + slider->label + " added");
                }
                else if (type == "text_box")
                {
                    auto text = std::make_shared<TextBoxInfo>();
                    text->id = juce::Uuid();
                    text->label = pyharpComponent["label"].toString().toStdString();
                    text->value = pyharpComponent["value"].toString().toStdString();

                    controlsInfo.push_back({ text->id, text });
                    // controlsInfo[text->id] = text;
                    uuidsInOrder.push_back(text->id);
                    LogAndDBG("Text: " + text->label + " added");
                }
                else if (type == "number_box")
                {
                    auto number_box = std::make_shared<NumberBoxInfo>();
                    number_box->id = juce::Uuid();
                    number_box->label = pyharpComponent["label"].toString().toStdString();
                    number_box->min = pyharpComponent["min"].toString().getFloatValue();
                    number_box->max = pyharpComponent["max"].toString().getFloatValue();
                    number_box->value = pyharpComponent["value"].toString().getFloatValue();

                    controlsInfo.push_back({ number_box->id, number_box });
                    // controlsInfo[number_box->id] = number_box;
                    uuidsInOrder.push_back(number_box->id);
                    LogAndDBG("Number Box: " + number_box->label + " added");
                }
                else if (type == "toggle")
                {
                    auto toggle = std::make_shared<ToggleInfo>();
                    toggle->id = juce::Uuid();
                    toggle->label = pyharpComponent["label"].toString().toStdString();
                    toggle->value = ("1" == pyharpComponent["value"].toString().toStdString());
                    // toggle->value = (aa == "1");
                    controlsInfo.push_back({ toggle->id, toggle });
                    // controlsInfo[toggle->id] = toggle;
                    uuidsInOrder.push_back(toggle->id);
                    LogAndDBG("Toggle: " + toggle->label + " added");
                }
                else if (type == "dropdown")
                {
                    auto dropdown = std::make_shared<ComboBoxInfo>();
                    dropdown->id = juce::Uuid();
                    dropdown->label = pyharpComponent["label"].toString().toStdString();
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

                // For the first two, we are abusing the term control.
                // They are actually the main inputs to the model (audio or midi)
                if (type == "audio_track")
                {
                    // CB:TODO: NOT USED ANYWHERE
                    // ControlAreaWidget.h ignores this when populating the GUI
                    auto audio_out = std::make_shared<AudioTrackInfo>();
                    audio_out->label = pyharpComponent["label"].toString().toStdString();
                    audio_out->id = juce::Uuid();

                    outputTracksInfo.push_back({ audio_out->id, audio_out });
                    // outputTracksInfo[audio_out->id] = audio_out;
                    // uuidsInOrder.push_back(audio_out->id);
                    LogAndDBG("Audio Out: " + audio_out->label + " added");
                }
                else if (type == "midi_track")
                {
                    auto midi_out = std::make_shared<MidiTrackInfo>();
                    midi_out->label = pyharpComponent["label"].toString().toStdString();
                    midi_out->id = juce::Uuid();

                    outputTracksInfo.push_back({ midi_out->id, midi_out });
                    // outputTracksInfo[midi_out->id] = midi_out;
                    // uuidsInOrder.push_back(midi_out->id);
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
        status2 = ModelStatus::LOADED;
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
            result = gradioClient.uploadFileRequest(std::get<2>(tuple), remoteTrackFilePath);
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

        juce::String eventId;
        juce::String endpoint = "process";
        // the jsonBody is created by controlsToJson
        juce::String processingPayload;
        result = prepareProcessingPayload(processingPayload);
        if (result.failed())
        {
            result.getError().devMessage = "Failed to upload file";
            status2 = ModelStatus::ERROR;
            return result;
        }
        // // TODO: The jsonBody should be created using DynamicObject and var
        // // TODO: make a utility function that wraps this into an object with a "data" key
        // // TODO: or maybe make this a part of controlsToJson
        // juce::String jsonBody = R"(
        //     {
        //         "data": )" + processingPayload
        //                         + R"(
        //     }
        //     )";

        status2 = ModelStatus::PROCESSING;
        result = gradioClient.makePostRequestForEventID(endpoint, eventId, processingPayload);
        if (result.failed())
        {
            result.getError().devMessage = "Failed to make post request.";
            status2 = ModelStatus::ERROR;
            return result;
        }

        juce::String response;
        result = gradioClient.getResponseFromEventID(endpoint, eventId, response, -1);
        if (result.failed())
        {
            result.getError().devMessage = "Failed to make get request";
            status2 = ModelStatus::ERROR;
            return result;
        }

        juce::String responseData;

        juce::String key = "data: ";
        result = gradioClient.extractKeyFromResponse(response, responseData, key);
        if (result.failed())
        {
            result.getError().devMessage = "Failed to extract 'data:'";
            status2 = ModelStatus::ERROR;
            return result;
        }

        juce::var parsedData;
        juce::JSON::parse(responseData, parsedData);
        if (! parsedData.isObject())
        {
            error.devMessage = "Failed to parse the 'data' key of the received JSON.";
            status2 = ModelStatus::ERROR;
            return OpResult::fail(error);
        }
        if (! parsedData.isArray())
        {
            error.devMessage = "Parsed data field should be an array.";
            status2 = ModelStatus::ERROR;
            return OpResult::fail(error);
        }
        juce::Array<juce::var>* dataArray = parsedData.getArray();
        if (dataArray == nullptr)
        {
            error.devMessage = "The data array is empty.";
            status2 = ModelStatus::ERROR;
            return OpResult::fail(error);
        }

        // Iterate through the array elements
        for (int i = 0; i < dataArray->size(); i++)
        {
            juce::var procObj = dataArray->getReference(i);
            if (! procObj.isObject())
            {
                status2 = ModelStatus::ERROR;
                error.devMessage =
                    "The " + juce::String(i)
                    + "th returned element of the process_fn function in the gradio-app is not an object.";
                return OpResult::fail(error);
            }
            // Make sure the object has a "meta" key
            // Gradio output components like File and Audio store metadata in the "meta" key
            // so we can use that to identify what kind of output it is
            if (! procObj.getDynamicObject())
            {
                status2 = ModelStatus::ERROR;
                error.devMessage =
                    "The " + juce::String(i)
                    + "th returned element of the process_fn function in the gradio-app is not a valid object. "
                    + "Make sure you are using LabelList() and not just a python list, in process_fn, to return the output labels.";
                return OpResult::fail(error);
            }
            if (! procObj.getDynamicObject()->hasProperty("meta"))
            {
                status2 = ModelStatus::ERROR;
                error.type = ErrorType::MissingJsonKey;
                error.devMessage =
                    "The " + juce::String(i)
                    + "th element of the array of processed outputs does not have a meta object. "
                    + "Make sure you are using LabelList() in process_fn to return the output labels.";
                return OpResult::fail(error);
            }
            juce::var meta = procObj.getDynamicObject()->getProperty("meta");
            // meta should be an object
            if (! meta.isObject())
            {
                status2 = ModelStatus::ERROR;
                error.type = ErrorType::MissingJsonKey;
                error.devMessage =
                    "The " + juce::String(i)
                    + "th element of the array of processed outputs does not have a valid meta object.";
                return OpResult::fail(error);
            }
            juce::String procObjType = meta.getDynamicObject()->getProperty("_type").toString();

            // procObjType could be "gradio.FileData" for file/midi/audio
            // and "pyharp.LabelList" for labels
            if (procObjType == "gradio.FileData")
            {
                // juce::String path = procObj.getDynamicObject()->getProperty("path").toString();
                juce::String outputFilePath;
                juce::String url = procObj.getDynamicObject()->getProperty("url").toString();

                result = gradioClient.downloadFileFromURL(url, outputFilePath);
                if (result.failed())
                {
                    status2 = ModelStatus::ERROR;
                    return result;
                }
                // Make a juce::File from the path
                // juce::File processedFile(outputFilePath);
                juce::File downloadedFile(outputFilePath);
                auto aa = downloadedFile.getFileName();
                auto bb = downloadedFile.getFullPathName();
                auto cc = URL(downloadedFile).toString(true);
                outputFilePaths.push_back(URL(downloadedFile).toString(true));

                // outputFilePaths.push_back(outputFilePath);
                // Replace the input file with the processed file
                // processedFile.moveFileTo(filetoProcess);
                // processedFile.moveFileTo(outputFile);
            }
            else if (procObjType == "pyharp.LabelList")
            {
                juce::Array<juce::var>* labelsPyharp =
                    procObj.getDynamicObject()->getProperty("labels").getArray();
                labels.clear();
                for (int j = 0; j < labelsPyharp->size(); j++)
                {
                    juce::DynamicObject* labelPyharp =
                        labelsPyharp->getReference(j).getDynamicObject();
                    juce::String labelType = labelPyharp->getProperty("label_type").toString();
                    std::unique_ptr<OutputLabel> label;

                    if (labelType == "AudioLabel")
                    {
                        auto audioLabel = std::make_unique<AudioLabel>();
                        if (labelPyharp->hasProperty("amplitude"))
                        {
                            if (labelPyharp->getProperty("amplitude").isDouble()
                                || labelPyharp->getProperty("amplitude").isInt())
                            {
                                audioLabel->amplitude =
                                    static_cast<float>(labelPyharp->getProperty("amplitude"));
                            }
                        }
                        label = std::move(audioLabel);
                    }
                    else if (labelType == "SpectrogramLabel")
                    {
                        auto spectrogramLabel = std::make_unique<SpectrogramLabel>();
                        if (labelPyharp->hasProperty("frequency"))
                        {
                            if (labelPyharp->getProperty("frequency").isDouble()
                                || labelPyharp->getProperty("frequency").isInt())
                            {
                                spectrogramLabel->frequency =
                                    static_cast<float>(labelPyharp->getProperty("frequency"));
                            }
                        }
                        label = std::move(spectrogramLabel);
                    }
                    else if (labelType == "MidiLabel")
                    {
                        auto midiLabel = std::make_unique<MidiLabel>();
                        if (labelPyharp->hasProperty("pitch"))
                        {
                            if (labelPyharp->getProperty("pitch").isDouble()
                                || labelPyharp->getProperty("pitch").isInt())
                            {
                                midiLabel->pitch =
                                    static_cast<float>(labelPyharp->getProperty("pitch"));
                            }
                        }
                        label = std::move(midiLabel);
                    }
                    else if (labelType == "OutputLabel")
                    {
                        auto outputLabel = std::make_unique<OutputLabel>();
                        label = std::move(outputLabel);
                    }
                    else
                    {
                        error.type = ErrorType::UnknownLabelType;
                        error.devMessage = "Unknown label type: " + labelType;
                        return OpResult::fail(error);
                    }
                    // All the labels, no matter theyr type, have some common properties
                    // t: float
                    // label: str
                    // duration: float = 0.0
                    // description: str = None
                    // color: int = 0
                    // first we'll check which of those exist and are not void or null
                    // for those that exist, we fill the struct properties
                    // the rest will be ignored
                    if (labelPyharp->hasProperty("t"))
                    {
                        // now check if it's a float
                        if (labelPyharp->getProperty("t").isDouble()
                            || labelPyharp->getProperty("t").isInt())
                        {
                            label->t = static_cast<float>(labelPyharp->getProperty("t"));
                        }
                    }
                    if (labelPyharp->hasProperty("label"))
                    {
                        // now check if it's a string
                        if (labelPyharp->getProperty("label").isString())
                        {
                            label->label = labelPyharp->getProperty("label").toString();
                            DBG("label: " + label->label);
                        }
                    }
                    if (labelPyharp->hasProperty("duration"))
                    {
                        // now check if it's a float
                        if (labelPyharp->getProperty("duration").isDouble()
                            || labelPyharp->getProperty("duration").isInt())
                        {
                            label->duration =
                                static_cast<float>(labelPyharp->getProperty("duration"));
                        }
                    }
                    if (labelPyharp->hasProperty("description"))
                    {
                        // now check if it's a string
                        if (labelPyharp->getProperty("description").isString())
                        {
                            label->description = labelPyharp->getProperty("description").toString();
                        }
                    }
                    if (labelPyharp->hasProperty("color"))
                    {
                        // now check if it's an int
                        if ((labelPyharp->getProperty("color").isInt64()
                             || labelPyharp->getProperty("color").isInt()))
                        {
                            int color_val = static_cast<int>(labelPyharp->getProperty("color"));

                            if (color_val != 0)
                            {
                                label->color = color_val;
                            }
                        }
                    }
                    if (labelPyharp->hasProperty("link"))
                    {
                        // now check if it's a string
                        if (labelPyharp->getProperty("link").isString())
                        {
                            label->link = labelPyharp->getProperty("link").toString();
                        }
                    }
                    labels.push_back(std::move(label));
                }
            }
            else
            {
                LogAndDBG("The pyharp Gradio app returned a " + procObjType
                          + " object, that we don't yet support in HARP.");
            }
        }
        // Finished status will be set by the MainComponent.h
        // status2 = ModelStatus::FINISHED;
        return result;
    }

    OpResult cancel()
    {
        // Create a successful result.
        // we'll update it to a failure result if something goes wrong
        OpResult result = OpResult::ok();

        juce::String eventId;
        juce::String endpoint = "cancel";

        // Perform a POST request to the cancel endpoint to get the event ID
        juce::String jsonBody = R"({"data": []})"; // The body is empty in this case

        status2 = ModelStatus::CANCELLING;
        result = gradioClient.makePostRequestForEventID(endpoint, eventId, jsonBody);
        if (result.failed())
        {
            status2 = ModelStatus::ERROR;
            return result;
        }

        // Use the event ID to make a GET request for the cancel response
        juce::String response;
        result = gradioClient.getResponseFromEventID(endpoint, eventId, response);
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

    GradioClient& getGradioClient() { return gradioClient; }

    LabelList& getLabels() { return labels; }

    std::vector<juce::String>& getOutputFilePaths() { return outputFilePaths; }

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
            // auto elementIt = (it1 != controlsInfo.end()) ? it1 : it2;

            // auto element = elementIt->second;

            // auto control = controlPair.second;
            // Check the type of control and extract its value
            if (auto sliderControl = dynamic_cast<SliderInfo*>(element.get()))
            {
                // Slider control, use sliderControl->value
                jsonControlsArray.add(juce::var(sliderControl->value));
            }
            else if (auto textBoxControl = dynamic_cast<TextBoxInfo*>(element.get()))
            {
                // Text box control, use textBoxControl->value
                jsonControlsArray.add(juce::var(textBoxControl->value));
            }
            else if (auto numberBoxControl = dynamic_cast<NumberBoxInfo*>(element.get()))
            {
                // Number box control, use numberBoxControl->value
                jsonControlsArray.add(juce::var(numberBoxControl->value));
            }
            else if (auto toggleControl = dynamic_cast<ToggleInfo*>(element.get()))
            {
                // Toggle control, use toggleControl->value
                jsonControlsArray.add(juce::var(toggleControl->value));
            }
            else if (auto comboBoxControl = dynamic_cast<ComboBoxInfo*>(element.get()))
            {
                // Combo box control, use comboBoxControl->value
                jsonControlsArray.add(juce::var(comboBoxControl->value));
            }
            else if (auto audioInTrackInfo = dynamic_cast<AudioTrackInfo*>(element.get()))
            {
                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty("path", juce::var(audioInTrackInfo->value));
                juce::DynamicObject::Ptr type = new juce::DynamicObject();
                type->setProperty("_type", juce::var("gradio.FileData"));
                obj->setProperty("meta", juce::var(type));

                jsonControlsArray.add(juce::var(obj));
            }
            else if (auto midiInTrackInfo = dynamic_cast<MidiTrackInfo*>(element.get()))
            {
                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty("path", juce::var(midiInTrackInfo->value));
                juce::DynamicObject::Ptr type = new juce::DynamicObject();
                type->setProperty("_type", juce::var("gradio.FileData"));
                obj->setProperty("meta", juce::var(type));

                jsonControlsArray.add(juce::var(obj));
            }
            else
            {
                Error error;
                error.type = ErrorType::UnsupportedControlType;
                // Unsupported control type or missing implementation
                error.devMessage =
                    "Unsupported control type or missing implementation for control with UUID: "
                    + currentUuid.toString();
                return OpResult::fail(error);
            }
        }

        // Create a DynamicObject to wrap the array with a "data" key
        juce::DynamicObject::Ptr dataObject = new juce::DynamicObject();
        dataObject->setProperty("data", jsonControlsArray);

        // Convert the entire object to a JSON string at the end
        payloadJson = juce::JSON::toString(juce::var(dataObject), true); // true for human-readable
        return OpResult::ok();
    }

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
    // juce::Array<juce::var> inputComponents;
    // juce::Array<juce::var> outputComponents;
    GradioClient gradioClient;

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
