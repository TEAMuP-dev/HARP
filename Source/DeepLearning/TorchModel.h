/**
 * @file

 * @brief Models defined in this file are any audio processing models that
 * utilize a libtorch backend for processing data.
 * @author hugo flores garcia, aldo aguilar, xribene
 */

#pragma once

#include <torch/script.h>
#include <torch/torch.h>
//#include <c10/core/impl/>
//#include <c10/util/Optional.h>
//#include <c10/core/impl/GenericDict.h>

#include <any>
#include <map>
#include <string>
#include <tuple>

#include "juce_audio_basics/juce_audio_basics.h"
// #include "../UI/ModelCard.h"
#include "Model.h"
#include "Wave2Wave.h"

using std::any;
using std::map;
using std::shared_ptr;
using std::unique_ptr;
using torch::jit::IValue;
using torch::jit::script::Module;

/**
 * @class TorchModel
 * @brief Class that represents a base TorchModel inherited from Model.
 */
class TorchModel : public Model
 {
public:
  TorchModel();

  // destructor
  ~TorchModel() override;

  //! loads a torchscript model from file.
  bool load(const string &modelPath) override;

  //! checks if a model is loaded onto memory.
  bool ready() const override;

  //! forward pass
  IValue forward(const std::vector<IValue> &inputs) const;

  static torch::Tensor to_tensor(const juce::AudioBuffer<float> &buffer);

  static bool to_buffer(const torch::Tensor &src_tensor,
                        juce::AudioBuffer<float> &dest_buffer);

private:
  std::string m_modelPath;

public: 
  unique_ptr<Module> m_model;


protected:
  bool m_loaded;
  mutable std::mutex m_mutex;

};

/**
 * @class WebWave2Wave
 * @brief Class that represents a WebWave2Wave inherited from TorchModel and
 * Wave2Wave.
 */
class WebWave2Wave : public TorchModel {
public:
  WebWave2Wave();

  void process(juce::AudioBuffer<float> *bufferToProcess, int sampleRate,  std::map<string, any> &params) const;
};

