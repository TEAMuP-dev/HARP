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
 * @author JUCE, aldo aguilar, hugo flores garcia
 */

#include "ProcessorEditor.h"

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

TensorJuceProcessorEditor::TensorJuceProcessorEditor(
    TensorJuceAudioProcessorImpl &p, EditorRenderer *er)
    : AudioProcessorEditor(&p), AudioProcessorEditorARAExtension(&p) {

  if (auto *editorView = getARAEditorView())
    documentView = std::make_unique<DocumentView>(*editorView, p.playHeadState);
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


  // initialize knobs
  InitGenericDial(dialA, "A", juce::Range<double>(0.0, 1.0), 0.01, 0.0);
  addAndMakeVisible(dialA);

  InitGenericDial(dialB, "B", juce::Range<double>(0.0, 1.0), 0.01, 0.0);
  addAndMakeVisible(dialB);

  InitGenericDial(dialC, "C", juce::Range<double>(0.0, 1.0), 0.01, 0.0);
  addAndMakeVisible(dialC);

  InitGenericDial(dialD, "D", juce::Range<double>(0.0, 1.0), 0.01, 0.0);
  addAndMakeVisible(dialD);

  // ARA requires that plugin editors are resizable to support tight integration
  // into the host UI
  setResizable(true, false);
  setSize(800, 300);

}

void TensorJuceProcessorEditor::buttonClicked(Button *button) {
  if (button == &processButton) {
    DBG("TensorJuceProcessorEditor::buttonClicked button listener activated");

    std::map<std::string, std::any> params = {
        {"A", dialA.getValue()},
        {"B", dialB.getValue()},
        {"C", dialC.getValue()},
        {"D", dialD.getValue()}};
    mEditorRenderer->executeProcess(params);
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
      mEditorRenderer->executeLoad(params);
    });
  }
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

  auto topArea = area.removeFromTop(area.getHeight() * 0.2);  // use the topArea for the mainBox layout

  juce::FlexBox knobBox;
  knobBox.flexWrap = juce::FlexBox::Wrap::noWrap;
  knobBox.justifyContent = juce::FlexBox::JustifyContent::spaceBetween; 

  knobBox.items.add(juce::FlexItem(dialA).withFlex(1));
  knobBox.items.add(juce::FlexItem(dialB).withFlex(1));
  knobBox.items.add(juce::FlexItem(dialC).withFlex(1));
  knobBox.items.add(juce::FlexItem(dialD).withFlex(1));

  juce::FlexBox buttonBox;
  buttonBox.flexDirection = juce::FlexBox::Direction::column;
  buttonBox.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;

  buttonBox.items.add(juce::FlexItem(processButton).withFlex(1));
  buttonBox.items.add(juce::FlexItem(loadModelButton).withFlex(1));

  juce::FlexBox mainBox;
  mainBox.flexDirection = juce::FlexBox::Direction::row;
  mainBox.items.add(juce::FlexItem(knobBox).withFlex(0.7));  // Adjust flex size according to your need
  mainBox.items.add(juce::FlexItem(buttonBox).withFlex(0.3));  // Adjust flex size according to your need

  mainBox.performLayout(topArea);  // use topArea instead of area

  if (documentView != nullptr) {
      documentView->setBounds(area);  // this should set the bounds correctly
  }
}
