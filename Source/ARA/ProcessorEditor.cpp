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
#include "../DeepLearning/WebModel.h"

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
  processButton.addListener(this);
  addAndMakeVisible(processButton);

  loadModelButton.setLookAndFeel(&buttonLookAndFeel);
  loadModelButton.setButtonText("Load model");
  loadModelButton.addListener(this);
  addAndMakeVisible(loadModelButton);

  // model path textbox
  modelPathTextBox.setMultiLine(false);
  modelPathTextBox.setReturnKeyStartsNewLine(false);
  modelPathTextBox.setReadOnly(false);
  modelPathTextBox.setScrollbarsShown(false);
  modelPathTextBox.setCaretVisible(true);
  modelPathTextBox.setText("path to a gradio endpoint");  // Default text
  addAndMakeVisible(modelPathTextBox);
  
  // model controls
  addAndMakeVisible(ctrlComponent);
  auto guiAttributes = mEditorView->getModelGuiAttributes();
  ctrlComponent.populateGui(guiAttributes);

  // model card component
  // Get the modelCard from the EditorView
  addAndMakeVisible(modelCardComponent); // TODO check when to do that
  modelCardComponent.setModelCard(mEditorView->getModelCard());

  // ARA requires that plugin editors are resizable to support tight integration
  // into the host UI
  setResizable(true, false);
  setSize(800, 300);
}

void TensorJuceProcessorEditor::buttonClicked(Button *button) {
  if (button == &processButton) {
    DBG("TensorJuceProcessorEditor::buttonClicked button listener activated");
    
    auto params = ctrlComponent.getParams();

    // mEditorRenderer->executeProcess(params);
    mDocumentController->executeProcess(params);
  }

  else if (button == &loadModelButton) {
    DBG("TensorJuceProcessorEditor::buttonClicked load model button listener activated");

    // collect input parameters for the model.
    std::map<std::string, std::any> params = {
      {"url", modelPathTextBox.getText().toStdString()},
    };

    resetUI();
    mDocumentController->executeLoad(params);

    // Model loading happens synchronously, so we can be sure that
    // the Editor View has the model card and UI attributes loaded
    modelCardComponent.setModelCard(mEditorView->getModelCard());

    auto guiAttributes = mEditorView->getModelGuiAttributes();
    ctrlComponent.populateGui(guiAttributes);
    resized();

  }
  else {
    DBG("a button was pressed, but we didn't do anything. ");
  }
}

// void TensorJuceProcessorEditor::modelCardLoaded(const ModelCard& card) {
//   modelCardComponent.setModelCard(card); // addAndMakeVisible(modelCardComponent); 
// }

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
  auto topArea = area.removeFromTop(area.getHeight() * 0.4);  // We'll keep the topArea for the grid layout

  juce::Grid grid;

  using Track = juce::Grid::TrackInfo;
  using Fr = juce::Grid::Fr;

  grid.templateRows = { Track(Fr(1)), Track(Fr(1)), Track(Fr(1)) };  // Three rows for the three buttons/box
  grid.templateColumns = { Track(Fr(1)), Track(Fr(2)), Track(Fr(1)) };  // Three columns for left, middle, right sections

  grid.items = { 
    juce::GridItem(loadModelButton), 
    juce::GridItem(ctrlComponent),  // Assuming you want this in the middle
    juce::GridItem(),  // Empty spot

    juce::GridItem(modelPathTextBox),
    juce::GridItem(),  // Empty spot
    juce::GridItem(),  // Empty spot

    juce::GridItem(processButton), 
    juce::GridItem(),  // Empty spot
    juce::GridItem(modelCardComponent)  // Placing modelCardComponent in the bottom right
  };

  grid.performLayout(topArea);

  if (documentView != nullptr) {
    documentView->setBounds(area);  // this should set the bounds correctly
  }
}
