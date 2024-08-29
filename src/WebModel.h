/**
 * @file
 * @brief Base class for any models that utilize a web api to process
 * information. Currently we provide an implmentation of a wave 2 wave web based
 * model. We use the gradio python client to communicate with a gradio server.
 * @author hugo flores garcia, aldo aguilar, xribene
 */

#pragma once

#include <fstream>
#include "Model.h"
#include "gradio/GradioClient.h"
#include "juce_core/juce_core.h"

class WebModel : public Model
{
public:

    WebModel() // TODO: should be a singleton
    { 
        // create our logger
        m_logger.reset(juce::FileLogger::createDefaultAppLogger("HARP", "webmodel.log", "hello, harp!"));
        m_status_flag_file.replaceWithText("Status.INITIALIZED");
    }

    ~WebModel()
    {
        // clean up flag files
        m_cancel_flag_file.deleteFile();
        m_status_flag_file.deleteFile();
    }

    void LogAndDBG(const juce::String& message) const
    {
        DBG(message);
        if (m_logger)
            m_logger->logMessage(message);

    }

    bool ready() const override { return m_loaded; }

    std::string space_url() const { return m_url; }

    juce::File getLogFile() const
    {
        return m_logger->getLogFile();
    }

    void load(const map<string, any> &params) override
    {
        m_ctrls.clear();
        m_loaded = false;

        // get the name of the huggingface repo we're going to use
        if (!modelparams::contains(params, "url"))
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
        
        if (!error.isEmpty())
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
        juce::Array<juce::var> *tags = cardDict.getProperty("tags").getArray();
        if (tags == nullptr) {
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
            if (!ctrl.isObject()) {
                throw std::runtime_error("Failed to load controls from JSON. ctrl is not an object.");
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

                    m_ctrls.push_back({audio_in->id, audio_in});
                    LogAndDBG("Audio In: " + audio_in->label + " added");
                }
                else if (ctrl_type == "midi_in") 
                {
                    auto midi_in = std::make_shared<MidiInCtrl>();
                    midi_in->label = ctrl["label"].toString().toStdString();

                    m_ctrls.push_back({midi_in->id, midi_in});
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

                    m_ctrls.push_back({slider->id, slider});
                    LogAndDBG("Slider: " + slider->label + " added");
                }
                else if (ctrl_type == "text")
                {
                    auto text = std::make_shared<TextBoxCtrl>();
                    text->id = juce::Uuid();
                    text->label = ctrl["label"].toString().toStdString();
                    text->value = ctrl["value"].toString().toStdString();

                    m_ctrls.push_back({text->id, text});
                    LogAndDBG("Text: " + text->label + " added");
                }
                else if (ctrl_type == "number_box")
                {
                    auto number_box = std::make_shared<NumberBoxCtrl>();
                    number_box->label = ctrl["label"].toString().toStdString();
                    number_box->min = ctrl["min"].toString().getFloatValue();
                    number_box->max = ctrl["max"].toString().getFloatValue();
                    number_box->value = ctrl["value"].toString().getFloatValue();

                    m_ctrls.push_back({number_box->id, number_box});
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
        m_status_flag_file.replaceWithText("Status.LOADED");
    }

    CtrlList& controls() { return m_ctrls; }

    void process(juce::File filetoProcess) const
    {
        juce::String error;
        juce::String uploadedFilePath;
        gradioClient.uploadFileRequest(filetoProcess, uploadedFilePath, error);
        if (!error.isEmpty())
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
        if (!error.isEmpty())
        {
            LogAndDBG(error);
            throw std::runtime_error(error.toStdString());
        }
        // TODO: The jsonBody should be created using DynamicObject and var
        // TODO: make a utility function that wraps this into an object with a "data" key
        // TODO: or maybe make this a part of ctrlsToJson
        juce::String jsonBody = R"(
            {
                "data": )" + ctrlJson + R"(
            }
            )";

        gradioClient.makePostRequestForEventID(endpoint, eventId, error, jsonBody);
        if (!error.isEmpty())
        {
            LogAndDBG(error);
            throw std::runtime_error(error.toStdString());
        }

        juce::String response;
        gradioClient.getResponseFromEventID(endpoint, eventId, response, error);
        if (!error.isEmpty())
        {
            LogAndDBG(error);
            throw std::runtime_error(error.toStdString());
        }

        juce::String responseData;
        juce::String key = "data: ";
        gradioClient.extractKeyFromResponse(response, responseData, key, error);
        if (!error.isEmpty())
        {
            DBG(error);
            return;
        }

        juce::var parsedData;
        juce::Result parseResult = juce::JSON::parse(responseData, parsedData);
        if (!parsedData.isObject())
        {
            error = "Failed to parse the data portion of the received controls JSON.";
            DBG(error);
            return;
        }
        if (!parsedData.isArray())
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
            if (!procObj.isObject())
            {
                error = "The " + juce::String(i) + "th element of the array of processed outputs is not an object.";
                DBG(error);
                return;
            }
            // Make sure the object has a "meta" key
            // Gradio output compoenents like File and Audio store metadata in the "meta" key
            // so we can use that to identify what kind of output it is
            juce::var meta = procObj.getDynamicObject()->getProperty("meta");
            // meta should be an object
            if (!meta.isObject())
            {
                error = "The " + juce::String(i) + "th element of the array of processed outputs does not have a meta object.";
                DBG(error);
                return;
            }
            juce::String procObjType = meta.getDynamicObject()->getProperty("_type").toString();

            // procObjType could be "gradio.FileData" for file/midi
            // "gradio.AudioData" for audio
            // and "pyharp.LabelList" for labels
            if (procObjType == "gradio.FileData")
            {
                juce::String path = procObj.getDynamicObject()->getProperty("path").toString();
                juce::String url = procObj.getDynamicObject()->getProperty("url").toString();
            }
            else if (procObjType == "pyharp.LabelList")
            {
                // DBG(procObjType);
                DBG("what");
            }
            else {
                error = "Unknown output type: " + procObjType;
                DBG(error);
                return;
            }
        }    
    }
        

    

    void process2(juce::File filetoProcess) const {
        // clear the cancel flag file
        m_cancel_flag_file.deleteFile();

        // make sure we're loaded
        LogAndDBG("WebModel::process");
        if (!m_loaded)
            throw std::runtime_error("Model not loaded");

        // Not sure if we need this. 
        // THis is from the ARA era.
        // Doesn't hurt to have it though.
        std::string randomString = juce::Uuid().toString().toStdString();

        // save the buffer to file
        LogAndDBG("Saving buffer to file");
        juce::File tempFile =
            juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getChildFile("input_" + randomString + ".mid");
        tempFile.deleteFile();
        // copy the file to a temp file
        filetoProcess.copyFileTo(tempFile);

        // a tarrget output file
        juce::File tempOutputFile =
            juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getChildFile("output_" + randomString + ".mid");
        tempOutputFile.deleteFile();

        // a ctrls file
        juce::File tempCtrlsFile =
            juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getChildFile("ctrls_" + randomString + ".json");
        tempCtrlsFile.deleteFile();

        LogAndDBG("saving controls...");
        if (!saveCtrls(tempCtrlsFile, tempFile.getFullPathName().toStdString()))
            throw std::runtime_error("Failed to save controls to file.");


        std::string command = (
            prefix_cmd
            + scriptPath.getFullPathName().toStdString()
            + " --mode process"
            + " --url " + m_url
            + " --output_path " + tempOutputFile.getFullPathName().toStdString()
            + " --ctrls_path " + tempCtrlsFile.getFullPathName().toStdString()
            + " --cancel_flag_path " + m_cancel_flag_file.getFullPathName().toStdString()
            + " --status_flag_path " + m_status_flag_file.getFullPathName().toStdString()
            // + " >> " + tempLogFile.getFullPathName().toStdString()   // redirect stdout to the temp log file
            // + " 2>&1"   // redirect stderr to the same file as stdout
        );
        LogAndDBG("Running command: " + command);
        std::pair<juce::String, int> cmd_result = std::pair<juce::String, int>("", 0);

        juce::String logContent = cmd_result.first;
        juce::uint32 result = cmd_result.second;
        LogAndDBG(logContent);

        if (result != 0)
        {
            // read the text from the temp log file.
            std::string message;
            // check for a generic Error: in the log content
            if (logContent.contains("Error:"))
            {
                // get the error message
                juce::StringArray lines;
                lines.addLines(logContent);
                for (auto line : lines)
                {
                    if (line.contains("Error:"))
                    {
                        message = line.toStdString();
                        break;
                    }
                }
            }
            else
            {
                message = "An error occurred while calling the gradiojuce helper with mode \'process\'. ";
            }

            message += "\n Check the logs " + m_logger->getLogFile().getFullPathName().toStdString() + " for more details.";
        }

        // move the temp output file to the original input file
        tempOutputFile.moveFileTo(filetoProcess);

        // delete the temp input file
        tempFile.deleteFile();
        tempOutputFile.deleteFile();
        tempCtrlsFile.deleteFile();
        LogAndDBG("WebModel::process done");

        // clear the cancel flag file
        m_cancel_flag_file.deleteFile();
        return;
    }

    // sets a cancel flag file that the client can check to see if the process
    // should be cancelled
    void cancel()
    {
        m_cancel_flag_file.deleteFile();
        m_cancel_flag_file.create();
    }

    std::string getStatus()
    {
        // if the status file doesn't exist, return Status.INACTIVE
        if (!m_status_flag_file.exists())
            return "Status.INACTIVE";

        // read the status file and return its text
        juce::String status = m_status_flag_file.loadFileAsString();
        return status.toStdString();
    }

    juce::File getCancelFlagFile() const
    {
        return m_cancel_flag_file;
    }

    CtrlList::iterator findCtrlByUuid(const juce::Uuid& uuid)
    {
        return std::find_if(m_ctrls.begin(), m_ctrls.end(),
            [&uuid](const CtrlList::value_type& pair) {
                return pair.first == uuid;
            }
        );
    }

    GradioClient& getGradioClient() 
    {
        return gradioClient;
    }
private:
    juce::var loadJsonFromFile(const juce::File& file) const {
        juce::var result;

        LogAndDBG("Loading JSON from file: " + file.getFullPathName());
        if (!file.existsAsFile())
        {
            LogAndDBG("File does not exist: " + file.getFullPathName());
            return result;
        }

        juce::String fileContent = file.loadFileAsString();

        juce::Result parseResult = juce::JSON::parse(fileContent, result);

        if (parseResult.failed())
        {
            LogAndDBG("Failed to parse JSON: " + parseResult.getErrorMessage());
            return juce::var();  // Return an empty var
        }

        return result;
    }

    void ctrlsToJson(
                juce::String& ctrlJson, 
                std::string mediaInputPath,
                juce::String& error) const 
        {
        // Create a JSON array to hold each control's value
        juce::Array<juce::var> jsonCtrlsArray;

        // Iterate through each control in m_ctrls
        for (const auto& ctrlPair : m_ctrls)
        {
            auto ctrl = ctrlPair.second;
            // Check the type of ctrl and extract its value
            if (auto sliderCtrl = dynamic_cast<SliderCtrl*>(ctrl.get())){
                // Slider control, use sliderCtrl->value
                jsonCtrlsArray.add(juce::var(sliderCtrl->value));
            } else if (auto textBoxCtrl = dynamic_cast<TextBoxCtrl*>(ctrl.get())) {
                // Text box control, use textBoxCtrl->value
                jsonCtrlsArray.add(juce::var(textBoxCtrl->value));
            } else if (auto numberBoxCtrl = dynamic_cast<NumberBoxCtrl*>(ctrl.get())) {
                // Number box control, use numberBoxCtrl->value
                jsonCtrlsArray.add(juce::var(numberBoxCtrl->value));
            } else if (auto toggleCtrl = dynamic_cast<ToggleCtrl*>(ctrl.get())) {
                // Toggle control, use toggleCtrl->value
                jsonCtrlsArray.add(juce::var(toggleCtrl->value));
            } else if (auto comboBoxCtrl = dynamic_cast<ComboBoxCtrl*>(ctrl.get())) {
                // Combo box control, use comboBoxCtrl->value
                jsonCtrlsArray.add(juce::var(comboBoxCtrl->value));
            } else if (auto audioInCtrl = dynamic_cast<AudioInCtrl*>(ctrl.get())) {
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

            } else if (auto midiInCtrl = dynamic_cast<MidiInCtrl*>(ctrl.get())) {
                midiInCtrl->value = mediaInputPath;
                // same as audioInCtrl
                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty("path", juce::var(midiInCtrl->value));
                jsonCtrlsArray.add(juce::var(obj));
            } else {
                // Unsupported control type or missing implementation
                error = "Unsupported control type or missing implementation for control with ID: " + ctrl->id.toString();
                LogAndDBG(error);
                return;
            }
        }

        // Convert the array to a JSON string
        ctrlJson = juce::JSON::toString(jsonCtrlsArray, true);  // true for human-readable
    }

    bool saveCtrls(juce::File savePath, std::string audioInputPath) const {
        // Create a JSON array to hold each control's value
        juce::Array<juce::var> jsonCtrlsArray;

        // Iterate through each control in m_ctrls
        for (const auto& ctrlPair : m_ctrls)
        {
            auto ctrl = ctrlPair.second;
            // Check the type of ctrl and extract its value
            if (auto sliderCtrl = dynamic_cast<SliderCtrl*>(ctrl.get())){
                // Slider control, use sliderCtrl->value
                jsonCtrlsArray.add(juce::var(sliderCtrl->value));
            } else if (auto textBoxCtrl = dynamic_cast<TextBoxCtrl*>(ctrl.get())) {
                // Text box control, use textBoxCtrl->value
                jsonCtrlsArray.add(juce::var(textBoxCtrl->value));
            } else if (auto numberBoxCtrl = dynamic_cast<NumberBoxCtrl*>(ctrl.get())) {
                // Number box control, use numberBoxCtrl->value
                jsonCtrlsArray.add(juce::var(numberBoxCtrl->value));
            } else if (auto toggleCtrl = dynamic_cast<ToggleCtrl*>(ctrl.get())) {
                // Toggle control, use toggleCtrl->value
                jsonCtrlsArray.add(juce::var(toggleCtrl->value));
            } else if (auto comboBoxCtrl = dynamic_cast<ComboBoxCtrl*>(ctrl.get())) {
                // Combo box control, use comboBoxCtrl->value
                jsonCtrlsArray.add(juce::var(comboBoxCtrl->value));
            } else if (auto audioInCtrl = dynamic_cast<AudioInCtrl*>(ctrl.get())) {
                // Audio in control, use audioInCtrl->value
                audioInCtrl->value = audioInputPath;
                jsonCtrlsArray.add(juce::var(audioInCtrl->value));
            } else if (auto midiInCtrl = dynamic_cast<MidiInCtrl*>(ctrl.get())) {
                midiInCtrl->value = audioInputPath;
                jsonCtrlsArray.add(juce::var(midiInCtrl->value));
            } else {
                // Unsupported control type or missing implementation
                LogAndDBG("Unsupported control type or missing implementation for control with ID: " + ctrl->id.toString());
                return false;
            }
        }

        // Convert the array to a JSON string
        juce::String jsonText = juce::JSON::toString(jsonCtrlsArray, true);  // true for human-readable

        // Write the JSON string to the specified file path
        if (!savePath.replaceWithText(jsonText))
        {
            LogAndDBG("Failed to save controls to file: " + savePath.getFullPathName());
            return false;
        }

        return true;
    }

    juce::File m_cancel_flag_file
    {
        juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("WebModel_CANCEL")
    };
    juce::File m_status_flag_file
    {
        juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("WebModel_STATUS")
    };

    CtrlList m_ctrls;
    std::unique_ptr<juce::FileLogger> m_logger {nullptr};

    string m_url;
    string prefix_cmd;
    juce::File scriptPath;
    GradioClient gradioClient;
};


// a timer that checks the status of the model and broadcasts a change if if there is one
class ModelStatusTimer : public juce::Timer,
                         public juce::ChangeBroadcaster
{
public:
    ModelStatusTimer(std::shared_ptr<WebModel> model) : m_model(model)
    {
    }

    void timerCallback() override
    {
        // get the status of the model
        std::string status = m_model->getStatus();

        // if the status has changed, broadcast a change
        if (status != m_last_status)
        {
            m_last_status = status;
            sendChangeMessage();
        }
    }

private:
    std::shared_ptr<WebModel> m_model;
    std::string m_last_status;
};
