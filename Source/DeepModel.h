#pragma once

#include <tuple>

#include <torch/script.h>
#include <torch/torch.h>

#include "juce_audio_basics/juce_audio_basics.h"

using std::unique_ptr;
using std::shared_ptr;
using torch::jit::IValue;
using torch::jit::script::Module;


class DeepModel
{
public:
   DeepModel() = default;

   //! loads a torchscript model from file. 
   bool load(const std::string &modelPath) {
        try {
             m_model = std::make_unique<Module>(torch::jit::load(modelPath));
             m_model->eval();
             m_loaded = true;
             
        }
        catch (const c10::Error &e) {
             std::cerr << "Error loading the model\n";
             std::cerr << e.what() << "\n";
             return false;
        }
        return true;
   }

   //! checks if a model is loaded onto memory. 
   bool is_loaded() const { return m_loaded; }

   //! forward pass 
   IValue forward(const std::vector<IValue> &inputs) const {
        return m_model->forward(inputs);
   }

    static torch::Tensor to_tensor(const juce::AudioBuffer<float> &buffer) {
        torch::TensorOptions tensorOptions = torch::TensorOptions()
                                                    .dtype(torch::kFloat32)
                                                    .device(torch::kCPU);
        auto tensor = torch::from_blob((void*)buffer.getArrayOfReadPointers(), {buffer.getNumChannels(), buffer.getNumSamples()}, tensorOptions);
        return tensor.clone();
    }
    
    // convert a tensor to an audio buffer
    // tensor must be shape (channels, samples)
    // todo: error handle
    static bool to_buffer(const torch::Tensor &src_tensor, juce::AudioBuffer<float> &dest_buffer)  {
        // make sure the tensor is shape (channels, samples)
        assert(src_tensor.dim() == 2);

        dest_buffer.setSize(static_cast<int>(src_tensor.size(0)), static_cast<int>(src_tensor.size(1)));

        // copy the tensor to the buffer
        for (int i = 0; i < dest_buffer.getNumChannels(); ++i) {
            auto dest_ptr = dest_buffer.getWritePointer(i);
            auto src_ptr = src_tensor[i].data_ptr<float>();
            std::copy(src_ptr, src_ptr + dest_buffer.getNumSamples(), dest_ptr);
        }
        return true;
    }

protected:
   unique_ptr<Module> m_model {nullptr};
   bool m_loaded {false};
};



class Resampler : DeepModel {
public:
    Resampler() = default;

    //! resample a waveform tensor. Waveform should be shape (channels, samples)
   torch::Tensor resample(const torch::Tensor &waveform, int sampleRateIn, int sampleRateOut) const {
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
};

   