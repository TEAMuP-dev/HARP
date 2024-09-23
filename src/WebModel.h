/**
 * @file
 * @brief Base class for any models that utilize a web api to process
 * information. Currently we provide an implmentation of a wave 2 wave web based
 * model. We use the gradio python client to communicate with a gradio server.
 * @author hugo flores garcia, aldo aguilar, xribene
 */

#pragma once

#include "Model.h"
#include "gradio/GradioClient.h"
#include "juce_core/juce_core.h"
#include <fstream>

using CtrlList = std::vector<std::pair<juce::Uuid, std::shared_ptr<Ctrl>>>;
using LabelList = std::vector<std::unique_ptr<OutputLabel>>;

class WebModel : public Model
{
public:
    WebModel() // TODO: should be a singleton
    {
        // create our logger
        m_logger.reset(
            juce::FileLogger::createDefaultAppLogger("HARP", "webmodel.log", "hello, harp!"));
        status = "Status.INITIALIZED";
    }

    ~WebModel() {}

    void LogAndDBG(const juce::String& message) const
    {
        DBG(message);
        if (m_logger)
            m_logger->logMessage(message);
    }

    bool ready() const override { return m_loaded; }

    juce::File getLogFile() const { return m_logger->getLogFile(); }

    CtrlList& controls() { return m_ctrls; }

    void load(const map<string, any>& params) override
    {
        m_ctrls.clear();
        m_loaded = false;

        // get the name of the huggingface repo we're going to use
        if (! modelparams::contains(params, "url"))
        {
            throw std::runtime_error("url not found in params");
        }

        std::string userSpaceAddress = std::any_cast<std::string>(params.at("url"));

        gradioClient.setSpaceInfo(userSpaceAddress);

        if (gradioClient.getSpaceInfo().status == SpaceInfo::Status::ERROR)
        {
            // LogAndDBG("Error: " + gradioClient.getSpaceInfo().error);
            LogAndDBG(gradioClient.getSpaceInfo().toString());
            throw std::runtime_error(gradioClient.getSpaceInfo().error.toStdString());
        }

        LogAndDBG(gradioClient.getSpaceInfo().toString());

        juce::Array<juce::var> ctrlList;
        juce::DynamicObject cardDict;
        juce::String error;
        gradioClient.getControls(ctrlList, cardDict, error);

        if (! error.isEmpty())
        {
            LogAndDBG(error);
            throw std::runtime_error(error.toStdString());
        }

        // TODO: probably need to check if these properties exist and if they're the right types.
        m_card = ModelCard();
        m_card.name = cardDict.getProperty("name").toString().toStdString();
        m_card.description = cardDict.getProperty("description").toString().toStdString();
        m_card.author = cardDict.getProperty("author").toString().toStdString();
        m_card.midi_in = (bool) cardDict.getProperty("midi_in");
        m_card.midi_out = (bool) cardDict.getProperty("midi_out");

        // tags is a list of str
        juce::Array<juce::var>* tags = cardDict.getProperty("tags").getArray();
        if (tags == nullptr)
        {
            throw std::runtime_error("Failed to load tags from JSON. tags is null.");
        }

        for (int i = 0; i < tags->size(); i++)
        {
            m_card.tags.push_back(tags->getReference(i).toString().toStdString());
        }
        // END MODELCARD

        // clear the m_ctrls vector
        m_ctrls.clear();

        // iterate through the list of controls
        // and add them to the m_ctrls vector
        for (int i = 0; i < ctrlList.size(); i++)
        {
            juce::var ctrl = ctrlList.getReference(i);
            if (! ctrl.isObject())
            {
                throw std::runtime_error(
                    "Failed to load controls from JSON. ctrl is not an object.");
            }

            try
            {
                // get the ctrl type
                juce::String ctrl_type = ctrl["ctrl_type"].toString().toStdString();

                // For the first two, we are abusing the term control.
                // They are actually the main inputs to the model (audio or midi)
                if (ctrl_type == "audio_in")
                {
                    auto audio_in = std::make_shared<AudioInCtrl>();
                    audio_in->label = ctrl["label"].toString().toStdString();

                    m_ctrls.push_back({ audio_in->id, audio_in });
                    LogAndDBG("Audio In: " + audio_in->label + " added");
                }
                else if (ctrl_type == "midi_in")
                {
                    auto midi_in = std::make_shared<MidiInCtrl>();
                    midi_in->label = ctrl["label"].toString().toStdString();

                    m_ctrls.push_back({ midi_in->id, midi_in });
                    LogAndDBG("MIDI In: " + midi_in->label + " added");
                }
                // The rest are the actual controls that map to hyperparameters
                // of the model
                else if (ctrl_type == "slider")
                {
                    auto slider = std::make_shared<SliderCtrl>();
                    slider->id = juce::Uuid();
                    slider->label = ctrl["label"].toString().toStdString();
                    slider->minimum = ctrl["minimum"].toString().getFloatValue();
                    slider->maximum = ctrl["maximum"].toString().getFloatValue();
                    slider->step = ctrl["step"].toString().getFloatValue();
                    slider->value = ctrl["value"].toString().getFloatValue();

                    m_ctrls.push_back({ slider->id, slider });
                    LogAndDBG("Slider: " + slider->label + " added");
                }
                else if (ctrl_type == "text")
                {
                    auto text = std::make_shared<TextBoxCtrl>();
                    text->id = juce::Uuid();
                    text->label = ctrl["label"].toString().toStdString();
                    text->value = ctrl["value"].toString().toStdString();

                    m_ctrls.push_back({ text->id, text });
                    LogAndDBG("Text: " + text->label + " added");
                }
                else if (ctrl_type == "number_box")
                {
                    auto number_box = std::make_shared<NumberBoxCtrl>();
                    number_box->label = ctrl["label"].toString().toStdString();
                    number_box->min = ctrl["min"].toString().getFloatValue();
                    number_box->max = ctrl["max"].toString().getFloatValue();
                    number_box->value = ctrl["value"].toString().getFloatValue();

                    m_ctrls.push_back({ number_box->id, number_box });
                    LogAndDBG("Number Box: " + number_box->label + " added");
                }
                else
                    LogAndDBG("failed to parse control with unknown type: " + ctrl_type);
            }
            catch (const char* e)
            {
                throw std::runtime_error("Failed to load controls from JSON. " + std::string(e));
            }
        }
        // outputPath.deleteFile();
        m_loaded = true;
        // set the status to LOADED
        status = "Status.LOADED";
    }

    void process(juce::File filetoProcess)
    {
        juce::String error;
        juce::String uploadedFilePath;
        gradioClient.uploadFileRequest(filetoProcess, uploadedFilePath, error);
        if (! error.isEmpty())
        {
            LogAndDBG(error);
            throw std::runtime_error(error.toStdString());
        }

        // Using the
        juce::String eventId;
        juce::String endpoint = "process";
        // the  jsonBody is created by ctrlsToJson
        juce::String ctrlJson;
        ctrlsToJson(ctrlJson, uploadedFilePath.toStdString(), error);
        if (! error.isEmpty())
        {
            LogAndDBG(error);
            throw std::runtime_error(error.toStdString());
        }
        // TODO: The jsonBody should be created using DynamicObject and var
        // TODO: make a utility function that wraps this into an object with a "data" key
        // TODO: or maybe make this a part of ctrlsToJson
        juce::String jsonBody = R"(
            {
                "data": )" + ctrlJson
                                + R"(
            }
            )";

        gradioClient.makePostRequestForEventID(endpoint, eventId, error, jsonBody);
        if (! error.isEmpty())
        {
            LogAndDBG(error);
            throw std::runtime_error(error.toStdString());
        }

        juce::String response;
        gradioClient.getResponseFromEventID(endpoint, eventId, response, error);
        if (! error.isEmpty())
        {
            LogAndDBG(error);
            throw std::runtime_error(error.toStdString());
        }

        juce::String responseData;
        juce::String key = "data: ";
        gradioClient.extractKeyFromResponse(response, responseData, key, error);
        if (! error.isEmpty())
        {
            DBG(error);
            return;
        }

        juce::var parsedData;
        juce::Result parseResult = juce::JSON::parse(responseData, parsedData);
        if (! parsedData.isObject())
        {
            error = "Failed to parse the data portion of the received controls JSON.";
            DBG(error);
            return;
        }
        if (! parsedData.isArray())
        {
            error = "Parsed data field should be an array.";
            DBG(error);
            return;
        }
        juce::Array<juce::var>* dataArray = parsedData.getArray();
        if (dataArray == nullptr)
        {
            error = "The data array is empty.";
            DBG(error);
            return;
        }

        // Iterate through the array elements
        for (int i = 0; i < dataArray->size(); i++)
        {
            juce::var procObj = dataArray->getReference(i);
            if (! procObj.isObject())
            {
                error = "The " + juce::String(i)
                        + "th element of the array of processed outputs is not an object.";
                DBG(error);
                return;
            }
            // Make sure the object has a "meta" key
            // Gradio output compoenents like File and Audio store metadata in the "meta" key
            // so we can use that to identify what kind of output it is
            juce::var meta = procObj.getDynamicObject()->getProperty("meta");
            // meta should be an object
            if (! meta.isObject())
            {
                error =
                    "The " + juce::String(i)
                    + "th element of the array of processed outputs does not have a meta object.";
                DBG(error);
                return;
            }
            juce::String procObjType = meta.getDynamicObject()->getProperty("_type").toString();

            // procObjType could be "gradio.FileData" for file/midi/audio
            // and "pyharp.LabelList" for labels
            if (procObjType == "gradio.FileData")
            {
                // juce::String path = procObj.getDynamicObject()->getProperty("path").toString();
                juce::String outputFilePath;
                juce::String url = procObj.getDynamicObject()->getProperty("url").toString();
                // First check if the gradio app is a localmodel or not
                // if it is, we leave the url/path as is
                // if not, we'll use the url, after we remove the substring
                // "/c/file=" with "/file="
                // Check if the url contains "space/c/file="
                if (url.contains("/c/file="))
                {
                    // Replace "space/c/file=" with "space/file="
                    url = url.replace("/c/file=", "/file=");
                }
                else
                {
                    error =
                        "The url does not contain the expected substring '/c/file='. Check if https://github.com/gradio-app/gradio/issues/9049 has been fixed";
                    DBG(error);
                    return;
                }
                gradioClient.downloadFileFromURL(url, outputFilePath, error);
                if (! error.isEmpty())
                {
                    DBG(error);
                    return;
                }
                // Make a juce::File from the path
                juce::File processedFile(outputFilePath);
                // Replace the input file with the processed file
                processedFile.moveFileTo(filetoProcess);
            }
            else if (procObjType == "pyharp.LabelList")
            {
                // LabelList labels;
                juce::Array<juce::var>* labelsPyharp =
                    procObj.getDynamicObject()->getProperty("labels").getArray();
                // LabelList labels;
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
                            if (labelPyharp->getProperty("amplitude").isDouble())
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
                            if (labelPyharp->getProperty("frequency").isDouble())
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
                            if (labelPyharp->getProperty("pitch").isDouble())
                            {
                                midiLabel->pitch =
                                    static_cast<float>(labelPyharp->getProperty("pitch"));
                            }
                        }
                        label = std::move(midiLabel);
                    }
                    else
                    {
                        error = "Unknown label type: " + labelType;
                        DBG(error);
                        return;
                    }
                    // All the labels, no matter theyr type, have some common properties
                    // t: float
                    // label: str
                    // duration: float = 0.0
                    // description: str = None
                    // first we'll check which of those exist and are not void or null
                    // for those that exist, we fill the struct properties
                    // the rest will be ignored
                    if (labelPyharp->hasProperty("t"))
                    {
                        // now check if it's a float
                        if (labelPyharp->getProperty("t").isDouble())
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
                        }
                    }
                    if (labelPyharp->hasProperty("duration"))
                    {
                        // now check if it's a float
                        if (labelPyharp->getProperty("duration").isDouble())
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
                    labels.push_back(std::move(label));
                }
            }
            else
            {
                error = "Unknown output type: " + procObjType;
                DBG(error);
                return;
            }
        }
    }

    // sets a cancel flag file that the client can check to see if the process
    // should be cancelled
    // void cancel(juce::String& error) const
    void cancel()
    {
        juce::String error;
        juce::String eventId;
        juce::String endpoint = "cancel";

        // Perform a POST request to the cancel endpoint to get the event ID
        juce::String jsonBody = R"({"data": []})"; // The body is empty in this case

        gradioClient.makePostRequestForEventID(endpoint, eventId, error, jsonBody);
        if (!error.isEmpty())
        {
            LogAndDBG(error);
            throw std::runtime_error(error.toStdString());
        }

        // Use the event ID to make a GET request for the cancel response
        juce::String response;
        gradioClient.getResponseFromEventID(endpoint, eventId, response, error);
        if (!error.isEmpty())
        {
            LogAndDBG(error);
            throw std::runtime_error(error.toStdString());
        }

        // Optionally log or process the response
        LogAndDBG("Cancel request completed. Response: " + response);
        status = "Status.CANCELLED";
    }

    juce::String getStatus() { return status; }

    CtrlList::iterator findCtrlByUuid(const juce::Uuid& uuid)
    {
        return std::find_if(m_ctrls.begin(),
                            m_ctrls.end(),
                            [&uuid](const CtrlList::value_type& pair)
                            { return pair.first == uuid; });
    }

    GradioClient& getGradioClient() { return gradioClient; }

    LabelList& getLabels() { return labels; }

private:
    void ctrlsToJson(juce::String& ctrlJson, std::string mediaInputPath, juce::String& error) const
    {
        // Create a JSON array to hold each control's value
        juce::Array<juce::var> jsonCtrlsArray;

        // Iterate through each control in m_ctrls
        for (const auto& ctrlPair : m_ctrls)
        {
            auto ctrl = ctrlPair.second;
            // Check the type of ctrl and extract its value
            if (auto sliderCtrl = dynamic_cast<SliderCtrl*>(ctrl.get()))
            {
                // Slider control, use sliderCtrl->value
                jsonCtrlsArray.add(juce::var(sliderCtrl->value));
            }
            else if (auto textBoxCtrl = dynamic_cast<TextBoxCtrl*>(ctrl.get()))
            {
                // Text box control, use textBoxCtrl->value
                jsonCtrlsArray.add(juce::var(textBoxCtrl->value));
            }
            else if (auto numberBoxCtrl = dynamic_cast<NumberBoxCtrl*>(ctrl.get()))
            {
                // Number box control, use numberBoxCtrl->value
                jsonCtrlsArray.add(juce::var(numberBoxCtrl->value));
            }
            else if (auto toggleCtrl = dynamic_cast<ToggleCtrl*>(ctrl.get()))
            {
                // Toggle control, use toggleCtrl->value
                jsonCtrlsArray.add(juce::var(toggleCtrl->value));
            }
            else if (auto comboBoxCtrl = dynamic_cast<ComboBoxCtrl*>(ctrl.get()))
            {
                // Combo box control, use comboBoxCtrl->value
                jsonCtrlsArray.add(juce::var(comboBoxCtrl->value));
            }
            else if (auto audioInCtrl = dynamic_cast<AudioInCtrl*>(ctrl.get()))
            {
                // Audio in control, use audioInCtrl->value
                audioInCtrl->value = mediaInputPath;
                // Due to the way gradio http api works, we need to add the mediaInputPath
                // into another object first, like this:
                // {
                //     "path": "path/to/audio/file"
                // }
                // juce::DynamicObject obj;
                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty("path", juce::var(audioInCtrl->value));
                // Then we add the object to the array
                jsonCtrlsArray.add(juce::var(obj));
            }
            else if (auto midiInCtrl = dynamic_cast<MidiInCtrl*>(ctrl.get()))
            {
                midiInCtrl->value = mediaInputPath;
                // same as audioInCtrl
                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty("path", juce::var(midiInCtrl->value));
                jsonCtrlsArray.add(juce::var(obj));
            }
            else
            {
                // Unsupported control type or missing implementation
                error = "Unsupported control type or missing implementation for control with ID: "
                        + ctrl->id.toString();
                LogAndDBG(error);
                return;
            }
        }

        // Convert the array to a JSON string
        ctrlJson = juce::JSON::toString(jsonCtrlsArray, true); // true for human-readable
    }

    CtrlList m_ctrls;
    std::unique_ptr<juce::FileLogger> m_logger { nullptr };
    GradioClient gradioClient;
    juce::String status;
    // A variable to store the latest labelList received during processing
    LabelList labels;
};

// a timer that checks the status of the model and broadcasts a change if if there is one
class ModelStatusTimer : public juce::Timer, public juce::ChangeBroadcaster
{
public:
    ModelStatusTimer(std::shared_ptr<WebModel> model) : m_model(model) {}

    void timerCallback() override
    {
        // get the status of the model
        juce::String status = m_model->getStatus();

        // if the status has changed, broadcast a change
        if (status != m_last_status)
        {
            m_last_status = status;
            sendChangeMessage();
        }
    }

private:
    std::shared_ptr<WebModel> m_model;
    juce::String m_last_status;
};
