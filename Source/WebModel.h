#pragma once 

#include <fstream>


#define PYBIND11_ASSERT_GIL_HELD_INCREF_DECREF 1

#include "Model.h"
#include <pybind11/embed.h>
namespace py = pybind11;
using namespace pybind11::literals;

class WebModel : public Model
{
public:
    virtual bool load(const map<string, any> &params) override {
        // get the name of the huggingface repo we're going to use
        if (!params.contains("url")) {
            return false;
        }
        if(!params.contains("api_name")){
            return false;
        }
        m_url = any_cast<string>(params.at("url"));
        m_api_name = any_cast<string>(params.at("api_name"));

        m_loaded = true;
        return true;
    }

    virtual bool ready() const override {
        return m_loaded;
    }

private:
    string m_url {};
    string m_api_name {};
    bool m_loaded = false;

};

class WebWave2Wave : public WebModel, public Wave2Wave
{
public:

    virtual void process(
        juce::AudioBuffer<float> *bufferToProcess, 
        int sampleRate, 
        const map<string, any> &kwargs
    ) const override {
        // save the buffer to file
        juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("input.wav");
        if (!save_buffer_to_file(*bufferToProcess, tempFile, sampleRate)) {
            DBG("Failed to save buffer to file.");
            return;
        }

        // a tarrget output file
        juce::File tempOutputFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("output.wav");

        // run the python script
        py::scoped_interpreter guard{}; 

        // Create a Python list to hold the command line arguments
        // Create a Python dictionary to hold the keyworded arguments
        pybind11::dict pykwargs;
        pykwargs["audio_path"] = tempFile.getFullPathName().toStdString();
        pykwargs["init_temp"] = 1.0;
        pykwargs["final_temp"] = 1.0;
        pykwargs["periodic_hint_freq"] = 7;
        pykwargs["periodic_hint_width"] = 1;
        pykwargs["num_steps"] = 1;



        try {
            // your pybind11 code
            py::object Client = py::module_::import("gradio_client").attr("Client");
            py::object client = Client(py::str("http://localhost:7860/"));
            DBG("Client created");

            string output_audio_path = client.attr("predict")(
                *(pykwargs.attr("values")()),
                "api_name"_a="/ez_vamp"
            ).cast<string>();
            py::print(output_audio_path);

            DBG("Predicted");

            // read the output file to a buffer
            // TODO: the sample rate should not be the incoming sample rate, but rather the 
            // output sample rate of the daw? 
            load_buffer_from_file(juce::File(output_audio_path), *bufferToProcess, sampleRate);

            // delete the temp input file
            tempFile.deleteFile();
            tempOutputFile.deleteFile();
        } catch (const py::error_already_set& e) {
            DBG("Exception: " << e.what());
            // Additional error handling or logging if needed
        }
        return;
    }

};
