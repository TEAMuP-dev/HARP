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
 * @brief The Processor Editor is the main interaction from users into the
 * plugin. We utilize the Processor Editor to manage the UI of the plugin and
 * control all user input. The Processor Editor manages the UI components for
 * user input, and manages a callback to send UI information to the deeplearning
 * model.
 * @author JUCE, aldo aguilar, hugo flores garcia, xribene
 */

#include "ProcessorEditor.h"
#include "../DeepLearning/TorchModel.h"
// using MyType = void (TensorJuceProcessorEditor::*)(std::string);


void TensorJuceProcessorEditor::InitGenericDial(
  juce::Slider &dial, const juce::String& valueSuffix, 
  const juce::Range<double> range, double step_size,
  float value
) {
  dial.setLookAndFeel(&toolbarSliderStyle);
  dial.setSliderStyle(Slider::Rotary);
  dial.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 90, 20);
  dial.setTextValueSuffix(valueSuffix);
  dial.setRange(range, step_size);
  dial.setValue(value);
  dial.addListener(this);
}

// destructor
// TensorJuceProcessorEditor::~TensorJuceProcessorEditor() {
//   // release listeners
//   // removeAllChangeListeners();
//   int aa = 42;
// }
TensorJuceProcessorEditor::TensorJuceProcessorEditor(
    TensorJuceAudioProcessorImpl &p, EditorRenderer *er)
    : AudioProcessorEditor(&p), AudioProcessorEditorARAExtension(&p) {

  if (auto *editorView = getARAEditorView()) {
  // mDocumentController = editorView.getDocumentController()//->getDocument<ARADocument>())
    mDocumentController = ARADocumentControllerSpecialisation::getSpecialisedDocumentController<
                                                    TensorJuceDocumentControllerSpecialisation>(
                                                    editorView->getDocumentController());
    documentView = std::make_unique<DocumentView>(*editorView, p.playHeadState);
  }
  mEditorRenderer = er;
  if (documentView != nullptr) {
    addAndMakeVisible(documentView.get());
  }

  // initialize your button
  processButton.setLookAndFeel(&buttonLookAndFeel);
  processButton.setButtonText("process");
  processButton.setColour(TextButton::buttonColourId, Colours::lightgrey);
  processButton.setColour(TextButton::textColourOffId, Colours::black);
  processButton.setColour(TextButton::buttonOnColourId, Colours::grey);
  processButton.setColour(TextButton::textColourOnId, Colours::black);
  processButton.addListener(this);
  addAndMakeVisible(processButton);


  // load model button
  loadModelButton.setLookAndFeel(&buttonLookAndFeel);
  loadModelButton.setButtonText("Load model");
  loadModelButton.setColour(TextButton::buttonColourId, Colours::lightgrey);
  loadModelButton.setColour(TextButton::textColourOffId, Colours::black);
  loadModelButton.setColour(TextButton::buttonOnColourId, Colours::grey);
  loadModelButton.setColour(TextButton::textColourOnId, Colours::black);
  loadModelButton.addListener(this);
  addAndMakeVisible(loadModelButton);

  // model card component
  addAndMakeVisible(modelCardComponent); // TODO check when to do that

  // ARA requires that plugin editors are resizable to support tight integration
  // into the host UI
  setResizable(true, false);
  setSize(800, 300);

}

void TensorJuceProcessorEditor::buttonClicked(Button *button) {
  if (button == &processButton) {
    DBG("TensorJuceProcessorEditor::buttonClicked button listener activated");
    // create an empty map
    std::map<std::string, std::any> params;
    for (int i = 0; i < continuousCtrls.size(); i++) {
      params.insert(std::pair<std::string, std::any>(
          continuousCtrls[i]->getName().toStdString(),
          continuousCtrls[i]->getValue()));
    }
    for (int i = 0; i < binaryCtrls.size(); i++) {
      params.insert(std::pair<std::string, std::any>(
          binaryCtrls[i]->getName().toStdString(),
          binaryCtrls[i]->getToggleState()));
    }
    for (int i = 0; i < textCtrls.size(); i++) {
      params.insert(std::pair<std::string, std::any>(
          textCtrls[i]->getName().toStdString(),
          textCtrls[i]->getText().toStdString()));
    }
    for (int i = 0; i < optionCtrls.size(); i++) {
      params.insert(std::pair<std::string, std::any>(
          optionCtrls[i]->getName().toStdString(),
          optionCtrls[i]->getText().toStdString()));
    }    
    // mEditorRenderer->executeProcess(params);
  }

  if (button == &loadModelButton) {
    DBG("TensorJuceProcessorEditor::buttonClicked load model button listener activated");

    modelPathChooser = std::make_unique<FileChooser> (
      "Please select the model file to load...",
      File::getSpecialLocation(File::userHomeDirectory),
      "*.pt"
    );

    auto folderChooserFlags = FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles;

    modelPathChooser->launchAsync (folderChooserFlags, [this] (const FileChooser& chooser)
    {
      File modelPath (chooser.getResult());

      DBG("got model path: " + modelPath.getFullPathName());
      // if  the file doesn't exist, return
      if (!modelPath.existsAsFile()) {
        return;
      }

      // convert modelPath to a string
      std::map<std::string, std::any> params = {
        {"modelPath", modelPath.getFullPathName().toStdString()}
      };

      // mEditorRenderer->getModel()->addMcListener(this);
      // mEditorRenderer->executeLoad(params, this); 
      // mDocumentController->printModelPath(modelPath.getFullPathName().toStdString());
      mDocumentController->getNeuralModel()->addModelCardListener(this);
      mDocumentController->getNeuralModel()->addListener(this);
      resetUI();
      mDocumentController->loadNeuralModel(params);
    });
    
  }
}

void TensorJuceProcessorEditor::modelCardLoaded(const ModelCard& card) {
  modelCardComponent.setModelCard(card); // addAndMakeVisible(modelCardComponent); 
}

void TensorJuceProcessorEditor::comboBoxChanged(ComboBox *box) {
}

void TensorJuceProcessorEditor::sliderValueChanged(Slider *slider) {
}

void TensorJuceProcessorEditor::paint(Graphics &g) {
  g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));

  if (!isARAEditorView()) {
    g.setColour(Colours::white);
    g.setFont(15.0f);
    g.drawFittedText(
        "ARA host isn't detected. This plugin only supports ARA mode",
        getLocalBounds(), Justification::centred, 1);
  }
}


void TensorJuceProcessorEditor::resized() {
  auto area = getLocalBounds();
  auto topArea = area.removeFromTop(area.getHeight() * 0.4);  // use the topArea for the mainBox layout
  juce::FlexBox mainBox;
      mainBox.flexDirection = juce::FlexBox::Direction::row;
      juce::FlexBox ctrlBox1;
          ctrlBox1.flexDirection = juce::FlexBox::Direction::column;
          juce::FlexBox continuousBox;
              // continuousBox.flexWrap = juce::FlexBox::Wrap::noWrap;
              // continuousBox.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
              // continuousBox.alignContent = juce::FlexBox::AlignContent::center;
              continuousBox.flexDirection = juce::FlexBox::Direction::row;
              for (int i = 0; i < continuousCtrls.size(); i++) {
                continuousBox.items.add(juce::FlexItem(*continuousCtrls[i]).withFlex(1));
              }
          ctrlBox1.items.add(juce::FlexItem(continuousBox).withFlex(1));
          juce::FlexBox binaryBox;
              binaryBox.flexDirection = juce::FlexBox::Direction::row;
              for (int i = 0; i < binaryCtrls.size(); i++) {
                binaryBox.items.add(juce::FlexItem(*binaryCtrls[i]).withFlex(1));
              }
          ctrlBox1.items.add(juce::FlexItem(binaryBox).withFlex(1));
      juce::FlexBox ctrlBox2;
          ctrlBox2.flexDirection = juce::FlexBox::Direction::column;
          juce::FlexBox optionBox;
              optionBox.flexDirection = juce::FlexBox::Direction::row;
              // optionBox.flexWrap = juce::FlexBox::Wrap::noWrap;
      //         optionBox.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
      //         optionBox.alignContent = juce::FlexBox::AlignContent::center;  
              for (int i = 0; i < optionCtrls.size(); i++) {
                optionBox.items.add(juce::FlexItem(*optionCtrls[i]).withFlex(1));
              }
              if (optionBox.items.size() > 0){
                  ctrlBox2.items.add(juce::FlexItem(optionBox).withFlex(1));
              }
          
          juce::FlexBox textCtrlBox;
              textCtrlBox.flexDirection = juce::FlexBox::Direction::row;
              for (int i = 0; i < textCtrls.size(); i++) {
                textCtrlBox.items.add(juce::FlexItem(*textCtrls[i]).withFlex(1));
              }
              if (textCtrlBox.items.size() > 0){
                  ctrlBox2.items.add(juce::FlexItem(textCtrlBox).withFlex(1));
              }
          // ctrlBox2.items.add(juce::FlexItem(textCtrlBox).withFlex(1));

      juce::FlexBox buttonBox;
          buttonBox.flexDirection = juce::FlexBox::Direction::column;
          buttonBox.items.add(juce::FlexItem(loadModelButton).withFlex(1));
          buttonBox.items.add(juce::FlexItem(processButton).withFlex(1));
      
      mainBox.items.add(juce::FlexItem(ctrlBox1).withFlex(0.3));
      mainBox.items.add(juce::FlexItem(ctrlBox2).withFlex(0.3));
      mainBox.items.add(juce::FlexItem(buttonBox).withFlex(0.3));

  juce::FlexBox superMainBox;
      superMainBox.flexDirection = juce::FlexBox::Direction::column;
      superMainBox.items.add(juce::FlexItem(modelCardComponent).withFlex(0.5));
      superMainBox.items.add(juce::FlexItem(mainBox).withFlex(0.5));
      
  superMainBox.performLayout(topArea);  // use topArea instead of area
  
  if (documentView != nullptr) {
      documentView->setBounds(area);  // this should set the bounds correctly
  }
}

void TensorJuceProcessorEditor::resetUI(){
  // remove all the widgets and empty the vectors
  for (auto &ctrl : continuousCtrls) {
    removeChildComponent(ctrl.get());
  }
  continuousCtrls.clear();
  for (auto &ctrl : binaryCtrls) {
    removeChildComponent(ctrl.get());
  }
  binaryCtrls.clear();
  for (auto &ctrl : textCtrls) {
    removeChildComponent(ctrl.get());
  }
  textCtrls.clear();
  for (auto &ctrl : optionCtrls) {
    removeChildComponent(ctrl.get());
  }
  optionCtrls.clear();
  // Also clear the model card component
  modelCardComponent.clear();
  resized();
}
// This callback gets called once a neuralModel is loaded
// It is used to create the UI elements for the controls
void TensorJuceProcessorEditor::changeListenerCallback(ChangeBroadcaster *source) {
  
  // cast the source to TorchModel
  auto tm = dynamic_cast<TorchModel *>(source);
  for (const auto &attr : tm->m_model->named_attributes()) {

    if (attr.name == "model_card") {
      // TODO : this if branch is useless after merging with hf/ctrl
      // auto model_card = attr.value.toObjectRef();
      // auto model_card2 = attr.value.toObject();
      // auto pycard = tm->m_model->attr("model_card").toObject();
      auto pycard2 = attr.value.toObject();

      std::string name = pycard2->getAttr("name").toStringRef();
      std::string description = pycard2->getAttr("description").toStringRef();
      std::string author = pycard2->getAttr("author").toStringRef();
      int sampleRate = pycard2->getAttr("sample_rate").toInt();
      for (const auto &tag : pycard2->getAttr("tags").toListRef()) {
        // m_card.tags.push_back(tag.toStringRef());
        DBG(tag.toStringRef());
      }
      
    }
    if (attr.name == "ctrls"){
      // make sure that the attribute is a dictionary, if not print a message
      if (!attr.value.isGenericDict()) {
        DBG("ctrls is not a dictionary");
        jassertfalse;
      }
      auto ctrls = attr.value.toGenericDict();
      for (auto it = ctrls.begin(); it != ctrls.end(); it++){

          auto nameId = it->key().toStringRef();
          auto ctrl = it->value().toObjectRef();
          auto name = ctrl.getAttr("name").toStringRef();
          std::string ctrlType = ctrl.getAttr("ctrl_type").toStringRef();
          
          auto widget = ctrl.getAttr("widget").toEnumHolder().get();
          //jassert(widget->isEnum());
          auto widgetType = widget->name();
          auto widgetTypeVal = widget->value().toInt();
          if (ctrlType == "ContinuousCtrl") {
              auto defaultValue = ctrl.getAttr("default").toDouble();
              auto min = ctrl.getAttr("min").toDouble();
              auto max = ctrl.getAttr("max").toDouble();
              auto step = ctrl.getAttr("step").toDouble();


              std::unique_ptr<juce::Slider> continuousCtrl = std::make_unique<juce::Slider>();

              // Customize the slider properties if needed
              if (widgetType == "SLIDER") {
                  continuousCtrl->setSliderStyle(juce::Slider::LinearHorizontal);
              }
              else if (widgetType == "ROTARY") {
                  continuousCtrl->setSliderStyle(juce::Slider::Rotary);
              }
              else {
                  DBG("Unknown widget type" + widgetType + " for ContinuousCtrl");
                  jassertfalse;
              }
              continuousCtrl->setName(nameId);
              continuousCtrl->setRange(min, max, step);
              continuousCtrl->setTextBoxStyle(juce::Slider::TextBoxAbove, false, 90, 20);
              continuousCtrl->addListener(this);
              continuousCtrl->setTextValueSuffix(" " + name);
              continuousCtrl->setLookAndFeel(&toolbarSliderStyle);
              addAndMakeVisible(*continuousCtrl);

              // Add the slider to the vector
              continuousCtrls.push_back(std::move(continuousCtrl));

          }
          else if (ctrlType == "BinaryCtrl") {
            auto defaultValue = ctrl.getAttr("default").toBool();
            if (widgetType == "CHECKBOX") {
                std::unique_ptr<juce::ToggleButton> binaryCtrl = std::make_unique<juce::ToggleButton>();
                binaryCtrl->setName(nameId);
                binaryCtrl->setButtonText(name);
                binaryCtrl->setToggleState(defaultValue, juce::dontSendNotification);
                binaryCtrl->addListener(this);
                addAndMakeVisible(*binaryCtrl);
                binaryCtrls.push_back(std::move(binaryCtrl));
            }
            else {
                DBG("Unknown widget type" + widgetType + " for ContinuousCtrl");
                jassertfalse;
            }
            
            
          }
          else if (ctrlType == "TextInputCtrl") {
              auto defaultValue = ctrl.getAttr("default").toStringRef();
              std::unique_ptr<TitledTextBox> textCtrl = std::make_unique<TitledTextBox>();
              textCtrl->setName(nameId);
              textCtrl->setTitle(name);
              textCtrl->setText(defaultValue);
              textCtrl->addListener(this);
              addAndMakeVisible(*textCtrl);
              textCtrls.push_back(std::move(textCtrl));
          }
          else if (ctrlType == "FloatInputCtrl") {
              auto defaultValue = ctrl.getAttr("default").toDouble();
          }
          else if (ctrlType == "OptionCtrl") {
              auto options = ctrl.getAttr("options").toListRef();
              // auto defaultValue = ctrl.getAttr("default").toString();
              std::unique_ptr<juce::ComboBox> optionCtrl = std::make_unique<juce::ComboBox>();
              for (auto &option : options) {
                optionCtrl->addItem(option.toStringRef(), optionCtrl->getNumItems() + 1);
              }
              optionCtrl->setSelectedId(1);
              optionCtrl->setName(nameId);
              optionCtrl->addListener(this);
              optionCtrl->setTextWhenNoChoicesAvailable("No choices");
              addAndMakeVisible(*optionCtrl);
              optionCtrls.push_back(std::move(optionCtrl));
          }
      

      }
    }
    }
  resized();
  DBG("ChangeListener");
}