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

#include "TorchModel.h"

namespace {
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
}


// Implementation of TorchModel methods

TorchModel::TorchModel() : m_model{nullptr}, m_loaded{false} {}

bool TorchModel::load(const map<string, any> &params) {
  if (modelparams::contains(params, "modelPath")) {
    return false;
  }
  auto modelPath = any_cast<string>(params.at("modelPath"));

  try {
    m_model = std::make_unique<Module>(torch::jit::load(modelPath));
    m_model->eval();
    m_loaded = true;

  } catch (const c10::Error &e) {
    std::cerr << "Error loading the model\n";
    std::cerr << e.what() << "\n";
    return false;
  }
  return true;
}

bool TorchModel::ready() const { return m_loaded; }

IValue TorchModel::forward(const std::vector<IValue> &inputs) const {
  return m_model->forward(inputs);
}

torch::Tensor TorchModel::to_tensor(const juce::AudioBuffer<float> &buffer) {
  torch::TensorOptions tensorOptions =
      torch::TensorOptions().dtype(torch::kFloat32).device(torch::kCPU);
  auto tensor = torch::from_blob(
      (void *)buffer.getArrayOfReadPointers(),
      {buffer.getNumChannels(), buffer.getNumSamples()}, tensorOptions);
  return tensor.clone();
}

bool TorchModel::to_buffer(const torch::Tensor &src_tensor,
                           juce::AudioBuffer<float> &dest_buffer) {
  // make sure the tensor is shape (channels, samples)
  assert(src_tensor.dim() == 2);

  dest_buffer.setSize(static_cast<int>(src_tensor.size(0)),
                      static_cast<int>(src_tensor.size(1)));

  // copy the tensor to the buffer
  for (int i = 0; i < dest_buffer.getNumChannels(); ++i) {
    auto dest_ptr = dest_buffer.getWritePointer(i);
    auto src_ptr = src_tensor[i].data_ptr<float>();
    std::copy(src_ptr, src_ptr + dest_buffer.getNumSamples(), dest_ptr);
  }
  return true;
}

// Implementation of TorchWave2Wave methods

TorchWave2Wave::TorchWave2Wave() {}

void TorchWave2Wave::process(juce::AudioBuffer<float> *bufferToProcess,
                             int sampleRate) const {
  // build our IValue (mixdown to mono for now)
  // TODO: support multichannel
  IValue input = {TorchModel::to_tensor(*bufferToProcess).mean(0, true)};
  DBG("built input tensor with shape "
      << size2string(input.toTensor().sizes()));

  // forward pass
  auto output = forward({input}).toTensor();
  DBG("got output tensor with shape " << size2string(output.sizes()));

  // we're expecting audio out
  TorchModel::to_buffer(output, *bufferToProcess);
  DBG("got output buffer with shape " << bufferToProcess->getNumChannels()
                                      << " x "
                                      << bufferToProcess->getNumSamples());
}

// Implementation of Resampler methods

Resampler::Resampler() {}

torch::Tensor Resampler::resample(const torch::Tensor &waveform,
                                  int sampleRateIn, int sampleRateOut) const {
  // early exit if the sample rates are the same
  if (sampleRateIn == sampleRateOut)
    return waveform;

  // set up inputs
  std::vector<torch::jit::IValue> inputs = {waveform,
                                            static_cast<float>(sampleRateIn),
                                            static_cast<float>(sampleRateOut)};
  try {
    return m_model->forward(inputs).toTensor();
  } catch (const c10::Error &e) {
    std::cerr << "Error resampling the waveform\n";
    return waveform;
  };
}
