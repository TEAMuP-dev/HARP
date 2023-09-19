/**
 * @file
 * @brief This file is part of the JUCE examples.
 * @brief Base class for any models that utilize a web api to process
 * information. Currently we provide an implmentation of a wave 2 wave web based
 * model. We use the gradio python client to communicate with a gradio server.
 * @author hugo flores garcia, aldo aguilar
 */

#pragma once

#include <fstream>

#define PYBIND11_ASSERT_GIL_HELD_INCREF_DECREF 1

#include "Model.h"
#include "Wave2Wave.h"
#include <Python.h>
#include <pybind11/embed.h>
namespace py = pybind11;
using namespace pybind11::literals;

namespace {
  std::wstring widen( const std::string& str )
  {
      using namespace std;
      wostringstream wstm ;
      const ctype<wchar_t>& ctfacet = use_facet<ctype<wchar_t>>(wstm.getloc()) ;
      for( size_t i=0 ; i<str.size() ; ++i ) 
                wstm << ctfacet.widen( str[i] ) ;
      return wstm.str() ;
  }
}

struct Ctrl {
  std::string name;
};

struct SliderCtrl : public Ctrl {
  float min;
  float max;
  float step;
  float value;
};

struct TextCtrl : public Ctrl {
  std::string value;
};

struct NumberBoxCtrl : public Ctrl {
  float min;
  float max;
  float value;
};


class WebWave2Wave : public Model, public Wave2Wave {
public:
  WebWave2Wave() { // TODO: should be a singleton

    py::initialize_interpreter();
    if (!Py_IsInitialized()) {
      DBG("Failed to initialize Python runtime.");
      return;
    }

    py::list sys_path = py::module_::import("sys").attr("path");

    for (auto path : sys_path) {
        DBG("Python sys.path (after setting): " << path.cast<std::string>());
    }

    // try to import the client. 
    DBG("Importing client");
    try {
      Client = py::module_::import("gradio_client").attr("Client");
    } catch (const py::error_already_set &e) {
      DBG("Exception: " << e.what());
      return;
    }
  }

  ~WebWave2Wave() {
    // finalize the python interpreter
    py::finalize_interpreter();
  }

  bool ready() const override { return m_loaded; }

  bool load(const map<string, any> &params) override {
    // get the name of the huggingface repo we're going to use
    if (!modelparams::contains(params, "url")) {
      DBG("url not found in params");
      return false;
    }

    try {
      DBG("loading model from " << any_cast<string>(params.at("url")));
      m_url = any_cast<string>(params.at("url"));
      m_loaded = true;

      DBG("establishing a client");
      m_client = Client(m_url, "verbose"_a = false);

      DBG("looking for a wav2wav api...");
      py::dict api = m_client.attr("view_api")("return_format"_a = "dict");

      py::dict named_endpoints = api["named_endpoints"];
      DBG("named_endpoints: " << py::str(named_endpoints.attr("keys")()));

      // check if the api has a wav2wav endpoint
      // otherwise we can't use this model
      if (!named_endpoints.contains("/wav2wav")) {
        DBG("wav2wav endpoint not found");
        return false;
      }

      // get the wav2wav endpoint
      py::dict w2w_endpoint = named_endpoints["/wav2wav"];
      DBG("w2w_endpoint: " << py::str(w2w_endpoint.attr("keys")()));

      // get the parameters of the wav2wav endpoint
      py::list w2w_parameters = w2w_endpoint["parameters"];

      // iterate over the parameters
      bool found_input_audio = false;
      for (py::handle handle : w2w_parameters) {
        py::dict param = handle.cast<py::dict>();
        DBG("param: " << py::str(param));

        // TODO: parse all of these into Ctrl objects
        if (param["component"].cast<string>() == "slider") {
          DBG("found slider");

          // construct a slider
          SliderCtrl ctrl;
          ctrl.name = param["name"].cast<string>();
          ctrl.min; // how? 
          ctrl.max; // how?
          ctrl.step = 0.01; // how?
  
        }

        

        // check if we have an input audio parameter
        if (param["name"].cast<string>() == "input audio") {
          DBG("found input audio");
        };
      }

      if (!found_input_audio) {
        DBG("error: input audio widget not found in the parameters for this endpoint!!!");
        return false;
      }
      
      // iterate over the returns, find one named "output audio"
      py::list w2w_returns = w2w_endpoint["returns"];
      bool found_output_audio = false;
      for (py::handle handle : w2w_returns) {
        py::dict ret = handle.cast<py::dict>();
        if (ret["name"].cast<string>() == "output audio") {
          DBG("found output audio");
          found_output_audio = true;
          break;
        }
      }

      if (!found_output_audio) {
        DBG("error: output audio widget not found in the return widgets for this endpoint!!!");
        return false;
      }

      return true; 
    }
    catch (const std::runtime_error &e) {
      DBG("Exception: " << e.what());
      return false;
    }
    catch (const py::error_already_set &e) {
      DBG("Exception: " << e.what());
      return false;
    }
  }

  virtual void process(juce::AudioBuffer<float> *bufferToProcess,
                       int sampleRate,
                       const map<string, any> &kwargs) const override {
    // make sure we're loaded
    if (!m_loaded) {
      DBG("Model not loaded");
      return;
    }
                    
    // save the buffer to file
    juce::File tempFile =
        juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("input.wav");
    if (!save_buffer_to_file(*bufferToProcess, tempFile, sampleRate)) {
      DBG("Failed to save buffer to file.");
      return;
    }

    // a tarrget output file
    juce::File tempOutputFile =
        juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("output.wav");

    // Create a Python list to hold the command line arguments
    // Create a Python dictionary to hold the keyworded arguments
    pybind11::dict pykwargs;
    try {
      pykwargs["use_coarse2fine"] = true;
      pykwargs["audio_path"] = tempFile.getFullPathName().toStdString();
      pykwargs["input_pitch_shift_semitones"] = 0;
      pykwargs["random_mask_intensity"] = 1.0;
      pykwargs["periodic_hint_freq"] =
          (int)any_cast<double>(kwargs.at("phint"));
      pykwargs["periodic_hint_width"] = 1;
      pykwargs["onset_mask_width"] = (int)any_cast<double>(kwargs.at("pwidth"));
      pykwargs["ncc"] = 0;
      pykwargs["stretch_factor"] = 1.0;
      pykwargs["prefix_hint"] = 0.0;
      pykwargs["suffix_hint"] = 0.0;
      pykwargs["init_temp"] = any_cast<double>(kwargs.at("temp1"));
      pykwargs["final_temp"] = any_cast<double>(kwargs.at("temp2"));
      pykwargs["num_steps"] = 36;
      pykwargs["mask_dropout"] = 0.0;
    } catch (const std::runtime_error &e) {
      DBG("Exception: " << e.what());
      return;
    }

    try {
      DBG("predicting");
      string output_audio_path =
          m_client.attr("predict")(*(pykwargs.attr("values")()),
                               "api_name"_a = "/vamp")
              .cast<string>();
      py::print(output_audio_path);
      DBG("Predicted");

      // read the output file to a buffer
      // TODO: the sample rate should not be the incoming sample rate, but
      // rather the output sample rate of the daw?
      load_buffer_from_file(juce::File(output_audio_path), *bufferToProcess,
                            sampleRate);

      // delete the temp input file
      tempFile.deleteFile();
      tempOutputFile.deleteFile();
    } catch (const py::error_already_set &e) {
      DBG("Exception: " << e.what());
      // Additional error handling or logging if needed
    }
    return;
  }

private:
  py::object Client;
  py::object m_client;
  string m_url;
  bool m_loaded;
};
