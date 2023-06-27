/**
 * @file
 * @brief This file is part of the JUCE examples.
 *
 * Copyright (c) 2022 - Raw Material Software Limited
 * The code included in this file is provided under the terms of the ISC license
 * http://www.isc.org/downloads/software-support-policy/isc-license. Permission
 * To use, copy, modify, and/or distribute this software for any purpose with or
 * without fee is hereby granted provided that the above copyright notice and
 * this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
 * WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
 * PURPOSE, ARE DISCLAIMED.
 *
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
#include <pybind11/embed.h>
namespace py = pybind11;
using namespace pybind11::literals;

class WebModel : public Model {
public:
  virtual bool load(const map<string, any> &params) override {
    // get the name of the huggingface repo we're going to use
    if (!params.contains("url")) {
      return false;
    }
    if (!params.contains("api_name")) {
      return false;
    }
    m_url = any_cast<string>(params.at("url"));
    m_api_name = any_cast<string>(params.at("api_name"));

    m_loaded = true;
    return true;
  }

  virtual bool ready() const override { return m_loaded; }

private:
  string m_url{};
  string m_api_name{};
  bool m_loaded = false;
};

class WebWave2Wave : public WebModel, public Wave2Wave {
private:
  py::object Client;

public:
  WebWave2Wave() { // TODO: should be a singleton
    // initialize the python interpreter
    py::initialize_interpreter();
    // your pybind11 code
    DBG("Importing client");
    Client = py::module_::import("gradio_client").attr("Client");
  }

  ~WebWave2Wave() {
    // finalize the python interpreter
    py::finalize_interpreter();
  }

  virtual void process(juce::AudioBuffer<float> *bufferToProcess,
                       int sampleRate,
                       const map<string, any> &kwargs) const override {
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
      DBG("creating client");
      py::object client = Client(py::str("http://localhost:7860/"));
      DBG("created client");

      DBG("predicting");
      string output_audio_path =
          client
              .attr("predict")(*(pykwargs.attr("values")()),
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
};
