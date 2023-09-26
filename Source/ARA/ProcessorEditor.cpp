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

  setLookAndFeel(&mHARPLookAndFeel);

  // initialize load and process buttons
  processButton.setButtonText("process");
  processButton.addListener(this);
  addAndMakeVisible(processButton);

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

  // TODO: what happens if the model is nullptr rn? 
  auto model = mEditorView->getModel();
  if (model == nullptr) {
    DBG("FATAL TensorJuceProcessorEditor::TensorJuceProcessorEditor: model is null");
    return;
  }
  
  // model controls
  ctrlComponent.setModel(mEditorView->getModel());
  addAndMakeVisible(ctrlComponent);
  ctrlComponent.populateGui();

  // model card component
  // Get the modelCard from the EditorView
  addAndMakeVisible(modelCardComponent); // TODO check when to do that
  modelCardComponent.setModelCard(mEditorView->getModel()->card());

  // ARA requires that plugin editors are resizable to support tight integration
  // into the host UI
  setResizable(true, false);
  setSize(800, 300);
}

void TensorJuceProcessorEditor::buttonClicked(Button *button) {
  if (button == &processButton) {
    DBG("TensorJuceProcessorEditor::buttonClicked button listener activated");
    
    auto model = mEditorView->getModel();
    mDocumentController->executeProcess(model);
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
    modelCardComponent.setModelCard(mEditorView->getModel()->card());
    ctrlComponent.setModel(mEditorView->getModel());
    ctrlComponent.populateGui();
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
    auto topArea = area.removeFromTop(area.getHeight() * 0.4);  // We'll keep the topArea for the vertical layout

    
    // Horizontal FlexBox for the top area
    juce::FlexBox fbTop;
    fbTop.flexDirection = juce::FlexBox::Direction::row;
    fbTop.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    fbTop.alignItems = juce::FlexBox::AlignItems::center;
    
    // Adding items to the top FlexBox
    fbTop.items.add(juce::FlexItem(loadModelButton).withFlex(0).withWidth(100).withHeight(30));  // Fixed width
    fbTop.items.add(juce::FlexItem(modelPathTextBox).withFlex(1).withHeight(30));  // Takes up remaining space
    
    // Horizontal FlexBox for the main area
    juce::FlexBox fbMain;
    fbMain.flexDirection = juce::FlexBox::Direction::row;
    fbMain.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    fbMain.alignItems = juce::FlexBox::AlignItems::stretch;
    
    // Adding items to the main FlexBox
    fbMain.items.add(juce::FlexItem(modelCardComponent).withFlex(0.4f));  // 40% width
    fbMain.items.add(juce::FlexItem(ctrlComponent).withFlex(0.6f));  // 60% width
    
    // Vertical FlexBox for overall layout
    juce::FlexBox fbOverall;
    fbOverall.flexDirection = juce::FlexBox::Direction::column;
    fbOverall.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    fbOverall.alignItems = juce::FlexBox::AlignItems::stretch;
    
    // Horizontal FlexBox for the bottom area
    juce::FlexBox fbBottom;
    fbBottom.flexDirection = juce::FlexBox::Direction::row;
    fbBottom.justifyContent = juce::FlexBox::JustifyContent::flexEnd;  // Aligns items to the right
    fbBottom.alignItems = juce::FlexBox::AlignItems::center;
    
    // Adding the processButton to the bottom FlexBox
    fbBottom.items.add(juce::FlexItem(processButton).withFlex(0).withWidth(100).withHeight(30));  // Fixed width and height

    // Adding items to the overall FlexBox
    fbOverall.items.add(juce::FlexItem(fbTop).withFlex(0).withHeight(20));  // Fixed height
    fbOverall.items.add(juce::FlexItem(fbMain).withFlex(1));  // Takes up remaining vertical space
    fbOverall.items.add(juce::FlexItem(fbBottom).withFlex(0).withHeight(30));  // Fixed height
    
    // Performing the layout
    fbOverall.performLayout(topArea);
    
    if (documentView != nullptr) {
        documentView->setBounds(area);  // This should set the bounds correctly
    }
}

