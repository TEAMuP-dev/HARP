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

TensorJuceProcessorEditor::TensorJuceProcessorEditor(
    ARADemoPluginAudioProcessorImpl &p, EditorRenderer *er)
    : AudioProcessorEditor(&p), AudioProcessorEditorARAExtension(&p) {

  if (auto *editorView = getARAEditorView())
    documentView = std::make_unique<DocumentView>(*editorView, p.playHeadState);
  mEditorRenderer = er;
  addAndMakeVisible(documentView.get());

  // initialize your button
  testButton.setLookAndFeel(&buttonLookAndFeel);
  testButton.setButtonText("generate");
  testButton.setColour(TextButton::buttonColourId, Colours::lightgrey);
  testButton.setColour(TextButton::textColourOffId, Colours::black);
  testButton.setColour(TextButton::buttonOnColourId, Colours::grey);
  testButton.setColour(TextButton::textColourOnId, Colours::black);
  testButton.addListener(this);
  addAndMakeVisible(testButton);

  // initalize knobs
  temp1Dial.setLookAndFeel(&toolbarSliderStyle);
  temp1Dial.setSliderStyle(Slider::Rotary);
  temp1Dial.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 90, 20);
  temp1Dial.setTextValueSuffix("t0: ");
  temp1Dial.setRange(0.0, 2.0, 0.01);
  temp1Dial.setValue(0.9);
  temp1Dial.addListener(this);
  addAndMakeVisible(temp1Dial);

  temp2Dial.setLookAndFeel(&toolbarSliderStyle);
  temp2Dial.setSliderStyle(Slider::Rotary);
  temp2Dial.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 90, 20);
  temp2Dial.setTextValueSuffix("t1: ");
  temp2Dial.setRange(0.0, 2.0, 0.01);
  temp2Dial.setValue(1.2);
  temp2Dial.addListener(this);
  addAndMakeVisible(temp2Dial);

  stepsDial.setLookAndFeel(&toolbarSliderStyle);
  stepsDial.setSliderStyle(Slider::Rotary);
  stepsDial.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 90, 20);
  stepsDial.setTextValueSuffix("steps: ");
  stepsDial.setRange(12, 64, 1);
  stepsDial.setValue(36);
  stepsDial.addListener(this);
  addAndMakeVisible(stepsDial);

  phintDial.setLookAndFeel(&toolbarSliderStyle);
  phintDial.setSliderStyle(Slider::Rotary);
  phintDial.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 90, 20);
  phintDial.setTextValueSuffix("phint: ");
  phintDial.setRange(1, 256, 1);
  phintDial.setValue(13);
  phintDial.addListener(this);
  addAndMakeVisible(phintDial);

  pwidthDial.setLookAndFeel(&toolbarSliderStyle);
  pwidthDial.setSliderStyle(Slider::Rotary);
  pwidthDial.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 90, 20);
  pwidthDial.setTextValueSuffix("pwidth: ");
  pwidthDial.setRange(0, 16, 1);
  pwidthDial.setValue(0);
  pwidthDial.addListener(this);
  addAndMakeVisible(pwidthDial);

  modeBox.setLookAndFeel(&comboBoxLookAndFeel);
  modeBox.addItem("daydream", 1);
  modeBox.addItem("deep sleep", 2);
  modeBox.setSelectedId(1);
  modeBox.addListener(this);
  addAndMakeVisible(modeBox);

  // ARA requires that plugin editors are resizable to support tight integration
  // into the host UI
  setResizable(true, false);
  setSize(800, 300);
}

void TensorJuceProcessorEditor::buttonClicked(Button *button) {
  if (button == &testButton) {
    DBG("TensorJuceProcessorEditor::buttonClicked button listener activated");

    std::map<std::string, std::any> params = {
        {"modeMode", modeBox.getText()},
        {"temp1", temp1Dial.getValue()},
        {"temp2", temp2Dial.getValue()},
        {"phint", phintDial.getValue()},
        {"pwidth", pwidthDial.getValue()}};
    mEditorRenderer->executeProcess(params);
  }
}

void TensorJuceProcessorEditor::comboBoxChanged(ComboBox *box) {
  if (box == &modeBox) {
    DBG("TensorJuceProcessorEditor::comboBoxChanged mode changed: "
        << box->getSelectedId());
  }
}

void TensorJuceProcessorEditor::sliderValueChanged(Slider *slider) {
  if (slider == &temp1Dial) {
    DBG("TensorJuceProcessorEditor::sliderValueChanged temp1 dial value:  "
        << slider->getValue());
  }

  else if (slider == &temp2Dial) {
    DBG("TensorJuceProcessorEditor::sliderValueChanged temp2 dial value:  "
        << slider->getValue());
  }
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
  int toolbarHeight = 70;
  int knobSize = 60;
  int buttonWidth = 100;
  int buttonHeight = 40;
  int comboWidth = 120;
  int comboHeight = 40;

  auto buttonArea = getLocalBounds().removeFromTop(toolbarHeight);
  int xCenter = (buttonArea.getWidth()) / 2;
  int yCenter = (toolbarHeight) / 2;

  // testButton.setBounds(xCenter-buttonWidth/2 - (buttonWidth/2 + knobSize),
  // yCenter-buttonHeight/2, buttonWidth, buttonHeight);
  modeBox.setBounds(xCenter - comboWidth - knobSize, yCenter - comboHeight / 2,
                    comboWidth, comboHeight);
  temp1Dial.setBounds(xCenter - knobSize / 2, yCenter - knobSize / 2, knobSize,
                      knobSize);
  temp2Dial.setBounds(xCenter - knobSize / 2 + knobSize, yCenter - knobSize / 2,
                      knobSize, knobSize);
  stepsDial.setBounds(xCenter - knobSize / 2 + (2 * knobSize),
                      yCenter - knobSize / 2, knobSize, knobSize);
  phintDial.setBounds(xCenter - knobSize / 2 + (3 * knobSize),
                      yCenter - knobSize / 2, knobSize, knobSize);
  pwidthDial.setBounds(xCenter - knobSize / 2 + (4 * knobSize),
                       yCenter - knobSize / 2, knobSize, knobSize);
  testButton.setBounds(xCenter - buttonWidth / 2 +
                           (buttonWidth / 2 + 5 * knobSize),
                       yCenter - buttonHeight / 2, buttonWidth, buttonHeight);

  if (documentView != nullptr)
    documentView->setBounds(0, toolbarHeight, getWidth(),
                            getHeight() - toolbarHeight);
}
