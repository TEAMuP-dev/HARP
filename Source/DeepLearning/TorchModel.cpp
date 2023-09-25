/**
 * @file
 * @brief Models defined in this file are any audio processing models that
 * utilize a libtorch backend for processing data.
 * @author hugo flores garcia, aldo aguilar, xribene
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

TorchModel::~TorchModel() {
  // release listeners
  removeAllChangeListeners();
}

bool TorchModel::load(const string &modelPath) {
  DBG("Loading model");
  std::cout<<"Loading model"<<std::endl;
  std::lock_guard<std::mutex> lock(m_mutex);
  // if (!modelparams::contains(params, "modelPath")) {
  //   DBG("modelPath not found in params");
  //   return false;
  // }
  // auto modelPath = any_cast<string>(params.at("modelPath"));
  DBG("Loading model from " + modelPath);

  // if this modelPath is the same as the one we already have loaded, (and we're already loaded)
  // then do nothing
  // if (m_loaded && modelPath == m_modelPath) {
  //   DBG("Model already loaded");
  //   return true;
  // }

  try {
    m_model = std::make_unique<Module>(torch::jit::load(modelPath));
    m_model->eval();
    m_loaded = true;
    DBG("Model loaded");

    // It's important to use the synchronous version of sendChangeMessage()
    sendSynchronousChangeMessage();

    // populate the model card
    // auto pycard = m_model->attr("model_card").toObject();
    // m_card.name = pycard->getAttr("name").toStringRef();
    // m_card.description = pycard->getAttr("description").toStringRef();
    // m_card.author = pycard->getAttr("author").toStringRef();
    // m_card.sampleRate = pycard->getAttr("sample_rate").toInt();
    // for (const auto &tag : pycard->getAttr("tags").toListRef()) {
    //   m_card.tags.push_back(tag.toStringRef());
    // }
    
  } catch (const char *e) {
    DBG("Error loading the model");
    std::cerr << "Error loading the model\n";
    std::cerr << e << "\n";
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

void TorchModel::addListener(juce::ChangeListener *listener) {
  addChangeListener(listener);
}

// Implementation of WebWave2Wave methodss

WebWave2Wave::WebWave2Wave() {}

void WebWave2Wave::process(juce::AudioBuffer<float> *bufferToProcess,
                             int sampleRate, std::map<string, any> &params) const {

  std::lock_guard<std::mutex> lock(m_mutex);

  // build our IValue (mixdown to mono for now)
  // TODO: support multichannel, right now we're downmixing to mono
  IValue input = {
    TorchModel::to_tensor(*bufferToProcess).mean(0, true)
  };

  // create a dict for our parameters
  c10::Dict<string, torch::Tensor> parameters; 
  auto keyType = c10::getTypePtr<std::string>(); // Example key type
  auto valueType = c10::getTypePtr<c10::IValue>(); // Example value type
  auto parameters2 = c10::impl::GenericDict(keyType, valueType);

  for (const auto& pair : params) {
      std::string key = pair.first;
      std::string valueAsString;
      double valueAsDouble = 0.0;

      if (pair.second.type() == typeid(int)) {
          valueAsString = std::to_string(std::any_cast<int>(pair.second));
          valueAsDouble = static_cast<double>(std::any_cast<int>(pair.second));
          parameters2.insert(key, IValue(torch::tensor({valueAsDouble})));
      } else if (pair.second.type() == typeid(float)) {
          valueAsString = std::to_string(std::any_cast<float>(pair.second));
          valueAsDouble = static_cast<double>(std::any_cast<float>(pair.second));
          parameters2.insert(key, torch::tensor({valueAsDouble}));
      } else if (pair.second.type() == typeid(double)) {
          valueAsString = std::to_string(std::any_cast<double>(pair.second));
          valueAsDouble = std::any_cast<double>(pair.second);
          parameters2.insert(key, torch::tensor({valueAsDouble}));
      } else if (pair.second.type() == typeid(bool)) {
          valueAsString = std::to_string(std::any_cast<bool>(pair.second));
          valueAsDouble = static_cast<double>(std::any_cast<bool>(pair.second));
          parameters2.insert(key, IValue(torch::tensor({valueAsDouble})));
      } else if (pair.second.type() == typeid(std::string)) {
          valueAsString = std::any_cast<std::string>(pair.second);
          // TODO: handle string parameters
          parameters2.insert(key, IValue(valueAsString));
      }

      DBG("{" << key << ": " << valueAsString << "}");
      parameters.insert(key, torch::tensor({valueAsDouble}));
  }

  std::this_thread::sleep_for(std::chrono::seconds(1));
  DBG("dummy sleep finished");
  // print input tensor shape
  DBG("built input audio tensor with shape "
      << size2string(input.toTensor().sizes()));
  // torch::Tensor aa = parameters.at("gain");
  // DBG("gain: " << aa.item<double>());
  // IValue bb = parameters2.at("gain");
  // torch::Tensor my_tensor = bb.toTensor();
  // DBG("gain3: " << my_tensor.item<double>());

  // IValue ss = parameters2.at("mode");
  // std::string mode = ss.toStringRef();
  // forward pass
  try {
    // resampling routine
    // DBG("resampling audio from " << sampleRate << " Hz" << " to " << card->getAttr("sample_rate").toInt() << " Hz");
    // DBG(
    //   "resampling audio from " << sampleRate << " Hz" << 
    //   " to " << m_card.sampleRate << " Hz"
    // );
    auto resampled = m_model->get_method("resample")({input, sampleRate}).toTensor();

    // perform the forward pass
    DBG("forward pass...");
    auto output = forward({resampled, parameters}).toTensor();
    DBG("got output tensor with shape " << size2string(output.sizes()));

    // we're expecting audio out
    DBG("converting output tensor to audio buffer");
    TorchModel::to_buffer(output, *bufferToProcess);
    DBG("got output buffer with shape " << bufferToProcess->getNumChannels()
                                        << " x "
                                        << bufferToProcess->getNumSamples());
    return;

  } catch (const char *msg) {
    std::cerr<<"Error processing audio: " << msg << "\n";
    DBG("Error processing audio: " << msg);
    return;
  }
}

