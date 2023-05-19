#pragma once

#include <tuple>

#include <torch/script.h>
#include <torch/torch.h>

#include "juce_audio_basics/juce_audio_basics.h"

#include "Model.h"

using std::unique_ptr;
using std::shared_ptr;
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


class TorchModel : public Model
{
public:
    TorchModel() = default;

    //! loads a torchscript model from file. 
    bool load(const map<string, any> &params) override {
        if (!params.contains("modelPath")) {
            return false;
        }
        auto modelPath = any_cast<string>(params.at("modelPath"));

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
    virtual bool ready() const override { return m_loaded; }

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


class TorchWave2Wave : public TorchModel, public Wave2Wave {
public:
    TorchWave2Wave() = default;

    void process(juce::AudioBuffer<float> *bufferToProcess, int sampleRate) const override { 
        // build our IValue (mixdown to mono for now)
        // TODO: support multichannel
        IValue input = {
            TorchModel::to_tensor(*bufferToProcess).mean(0, true)
        };
        DBG("built input tensor with shape " << size2string(input.toTensor().sizes()));

        // forward pass
        auto output = forward({input}).toTensor();
        DBG("got output tensor with shape " << size2string(output.sizes()));

        // we're expecting audio out
        TorchModel::to_buffer(output, *bufferToProcess);
        DBG("got output buffer with shape " << bufferToProcess->getNumChannels() << " x " << bufferToProcess->getNumSamples());

    }
};


class Resampler : TorchModel {
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

   