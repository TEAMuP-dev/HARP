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
//   processButton.setLookAndFeel (nullptr);
// }

TensorJuceProcessorEditor::TensorJuceProcessorEditor(
    TensorJuceAudioProcessorImpl &ap, EditorRenderer *er, 
    PlaybackRenderer *pr, EditorView *ev)
    : AudioProcessorEditor(&ap), AudioProcessorEditorARAExtension(&ap) {
  
  mEditorRenderer = er;
  mPlaybackRenderer = pr;
  mEditorView = ev;
  if (mEditorView != nullptr) {
    mDocumentController = ARADocumentControllerSpecialisation::getSpecialisedDocumentController<
                                                    TensorJuceDocumentControllerSpecialisation>(
                                                    mEditorView->getDocumentController());
    documentView = std::make_unique<DocumentView>(*mEditorView, ap.playHeadState);
  }

  if (documentView != nullptr) {
    addAndMakeVisible(documentView.get());
  }

  // initialize load and process buttons
  processButton.setLookAndFeel(&buttonLookAndFeel);
  processButton.setButtonText("process");
  processButton.setColour(TextButton::buttonColourId, Colours::lightgrey);
  processButton.setColour(TextButton::textColourOffId, Colours::black);
  processButton.setColour(TextButton::buttonOnColourId, Colours::grey);
  processButton.setColour(TextButton::textColourOnId, Colours::black);
  processButton.addListener(this);
  addAndMakeVisible(processButton);
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

  // Get the modelCard from the EditorView
  modelCardComponent.setModelCard(mEditorView->getModelCard());
  // populate the UI
  populateGui();
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
    mDocumentController->executeProcess(params);
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
      // std::map<std::string, std::any> params = {
      //   {"modelPath", modelPath.getFullPathName().toStdString()}
      // };

      resetUI();
      mDocumentController->executeLoad(modelPath.getFullPathName().toStdString());
      // Model loading happens synchronously, so we can be sure that
      // the Editor View has the model card and UI attributes loaded
      modelCardComponent.setModelCard(mEditorView->getModelCard());
      populateGui();
      resized();
    });
    
  }
}

// void TensorJuceProcessorEditor::modelCardLoaded(const ModelCard& card) {
//   modelCardComponent.setModelCard(card); // addAndMakeVisible(modelCardComponent); 
// }

void TensorJuceProcessorEditor::comboBoxChanged(ComboBox *box) {
  ignoreUnused(box);
}

void TensorJuceProcessorEditor::sliderValueChanged(Slider *slider) {
  ignoreUnused(slider);
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
// This callback gets called once a model is loaded
// It is used to create the UI elements for the controls
void TensorJuceProcessorEditor::populateGui() {
  
  for (const auto &ctrlAttributes  : mEditorView->getModelGuiAttributes()) {
    std::string nameId = std::any_cast<std::string>(ctrlAttributes.at("nameId"));
    std::string name = std::any_cast<std::string>(ctrlAttributes.at("name"));
    std::string widgetType = std::any_cast<std::string>(ctrlAttributes.at("widget_type"));
    // int widgetTypeVal = std::any_cast<int>(ctrlAttributes.at("widget_type_val"));
    std::string ctrlType = std::any_cast<std::string>(ctrlAttributes.at("ctrl_type"));
    if (ctrlType == "ContinuousCtrl") {
      // double defaultValue = std::any_cast<double>(ctrlAttributes.at("default"));
      double min = std::any_cast<double>(ctrlAttributes.at("min"));
      double max = std::any_cast<double>(ctrlAttributes.at("max"));
      double step = std::any_cast<double>(ctrlAttributes.at("step"));
      std::unique_ptr<juce::Slider> continuousCtrl = std::make_unique<juce::Slider>();
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
      bool defaultValue = std::any_cast<bool>(ctrlAttributes.at("default"));
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
        std::string defaultValue = std::any_cast<std::string>(ctrlAttributes.at("default"));
        std::unique_ptr<TitledTextBox> textCtrl = std::make_unique<TitledTextBox>();
        textCtrl->setName(nameId);
        textCtrl->setTitle(name);
        textCtrl->setText(defaultValue);
        textCtrl->addListener(this);
        addAndMakeVisible(*textCtrl);
        textCtrls.push_back(std::move(textCtrl));
    }
    else if (ctrlType == "FloatInputCtrl") {
        // double defaultValue = std::any_cast<double>(ctrlAttributes.at("default"));
        // TODO
    }
    else if (ctrlType == "OptionCtrl") {
        auto options = std::any_cast<std::vector<std::string>>(ctrlAttributes.at("options"));
        std::unique_ptr<juce::ComboBox> optionCtrl = std::make_unique<juce::ComboBox>();
        for (auto &option : options) {
          optionCtrl->addItem(option, optionCtrl->getNumItems() + 1);
        }
        optionCtrl->setSelectedId(1);
        optionCtrl->setName(nameId);
        optionCtrl->addListener(this);
        optionCtrl->setTextWhenNoChoicesAvailable("No choices");
        addAndMakeVisible(*optionCtrl);
        optionCtrls.push_back(std::move(optionCtrl));
    }
  }
  DBG("populateGui finished");
}