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
 * @brief Models defined in this file are any audio processing models that
 * utilize a libtorch backend for processing data.
 * @author hugo flores garcia, aldo aguilar
 */

#pragma once

#include <torch/script.h>
#include <torch/torch.h>

#include <any>
#include <map>
#include <string>
#include <tuple>

#include "juce_audio_basics/juce_audio_basics.h"

#include "Model.h"
#include "Wave2Wave.h"

using std::any;
using std::map;
using std::shared_ptr;
using std::unique_ptr;
using torch::jit::IValue;
using torch::jit::script::Module;

std::string size2string(torch::IntArrayRef size) {
  std::stringstream ss;
  ss << "(";
  for (int i = 0; i < size.size(); i++) {
    ss << size[i];
    if (i < size.size() - 1) {
      ss << ", ";
    }
  }
  ss << ")";
  return ss.str();
}

/**
 * @class TorchModel
 * @brief Class that represents a base TorchModel inherited from Model.
 */
class TorchModel : public Model {
public:
  TorchModel();

  //! loads a torchscript model from file.
  bool load(const map<string, any> &params) override;

  //! checks if a model is loaded onto memory.
  bool ready() const override;

  //! forward pass
  IValue forward(const std::vector<IValue> &inputs) const;

  static torch::Tensor to_tensor(const juce::AudioBuffer<float> &buffer);

  static bool to_buffer(const torch::Tensor &src_tensor,
                        juce::AudioBuffer<float> &dest_buffer);

protected:
  unique_ptr<Module> m_model;
  bool m_loaded;
};

/**
 * @class TorchWave2Wave
 * @brief Class that represents a TorchWave2Wave inherited from TorchModel and
 * Wave2Wave.
 */
class TorchWave2Wave : public TorchModel, public Wave2Wave {
public:
  TorchWave2Wave();

  void process(juce::AudioBuffer<float> *bufferToProcess, int sampleRate) const;
};

/**
 * @class Resampler
 * @brief Class that represents a Resampler inherited from TorchModel.
 */
class Resampler : public TorchModel {
public:
  Resampler();

  //! resample a waveform tensor. Waveform should be shape (channels, samples)
  torch::Tensor resample(const torch::Tensor &waveform, int sampleRateIn,
                         int sampleRateOut) const;
};
