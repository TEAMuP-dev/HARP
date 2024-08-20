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
#include "GradioAPI.h"
#include "juce_core/juce_core.h"


struct Ctrl 
{
    juce::Uuid id {""};
    std::string label {""};
    virtual ~Ctrl() = default; // virtual destructor
};

struct SliderCtrl : public Ctrl
{
    double minimum;
    double maximum;
    double step;
    double value;
};

struct TextBoxCtrl : public Ctrl 
{
    std::string value;
};

struct AudioInCtrl : public Ctrl 
{
    std::string value;
};

struct MidiInCtrl : public Ctrl 
{
    std::string value;
};

struct NumberBoxCtrl : public Ctrl 
{
    double min;
    double max;
    double value;
};

struct ToggleCtrl : public Ctrl
{
    bool value;
};

struct ComboBoxCtrl : public Ctrl
{
    std::vector<std::string> options;
    std::string value;
};


juce::String resolveSpaceUrl(juce::String urlOrName) {
    if (urlOrName.contains("localhost") || urlOrName.contains("huggingface.co") || urlOrName.contains("http")) {
        // do nothing! the url is already valid
    }
    else {
        DBG("HARPProcessorEditor::buttonClicked: spaceUrl is not a valid url");
        urlOrName = "https://huggingface.co/spaces/" + urlOrName;
    }
    return urlOrName;
}

using CtrlList = std::vector<std::pair<juce::Uuid, std::shared_ptr<Ctrl>>>;

namespace{
}

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
        if (m_logger) {
        m_logger->logMessage(message);
        }
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

        std::string url = std::any_cast<std::string>(params.at("url"));
        m_url = url; // Store the URL for future use
        LogAndDBG("url: " + m_url);

        juce::URL endpoint = juce::URL ("http://127.0.0.1:7860/call/wav2wav-ctrls");

        juce::Array<juce::var> ctrlList;
        juce::DynamicObject cardDict;
        juce::String error;
        getControls(endpoint, ctrlList, cardDict, error);
        
        if (!error.isEmpty())
        {
            DBG("Error: " << error);
            error += "\n Check the logs " + m_logger->getLogFile().getFullPathName() + " for more details.";
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

    void process(juce::File filetoProcess) const {
        // clear the cancel flag file
        m_cancel_flag_file.deleteFile();

        // make sure we're loaded
        LogAndDBG("WebModel::process");
        if (!m_loaded)
            throw std::runtime_error("Model not loaded");

        // a random string to append to the input/output.mid files
        // This is necessary because more than 1 playback regions
        // are processed at the same time.
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
