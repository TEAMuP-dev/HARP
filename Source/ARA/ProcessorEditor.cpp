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

HARPProcessorEditor::HARPProcessorEditor(
    HARPAudioProcessorImpl &ap, EditorRenderer *er, 
    PlaybackRenderer *pr, EditorView *ev)
    : AudioProcessorEditor(&ap), AudioProcessorEditorARAExtension(&ap) {
  
  mEditorRenderer = er;
  mPlaybackRenderer = pr;
  mEditorView = ev;
  if (mEditorView != nullptr) {
    mDocumentController = ARADocumentControllerSpecialisation::getSpecialisedDocumentController<
                                                    HARPDocumentControllerSpecialisation>(
                                                    mEditorView->getDocumentController());
    documentView = std::make_unique<DocumentView>(*mEditorView, ap.playHeadState);
  }

  if (documentView != nullptr) {
    addAndMakeVisible(documentView.get());
  }
  juce::LookAndFeel::setDefaultLookAndFeel (&mHARPLookAndFeel);

  // setLookAndFeel(&mHARPLookAndFeel);

  // initialize load and process buttons
  processButton.setButtonText("process");
  processButton.addListener(this);
  addAndMakeVisible(processButton);

  loadModelButton.setButtonText("load");
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
    DBG("FATAL HARPProcessorEditor::HARPProcessorEditor: model is null");
    return;
  }
  
  // model controls
  ctrlComponent.setModel(mEditorView->getModel());
  addAndMakeVisible(ctrlComponent);
  ctrlComponent.populateGui();

  addAndMakeVisible(nameLabel);
  addAndMakeVisible(authorLabel);
  addAndMakeVisible(descriptionLabel);
  addAndMakeVisible(tagsLabel);

  // model card component
  // Get the modelCard from the EditorView
  auto &card = mEditorView->getModel()->card();
  setModelCard(card);

  // ARA requires that plugin editors are resizable to support tight integration
  // into the host UI
  setResizable(true, false);
  setSize(800, 500);
}

void HARPProcessorEditor::setModelCard(const ModelCard& card) {
  // Set the text for the labels
  nameLabel.setText(juce::String(card.name), juce::dontSendNotification);
  descriptionLabel.setText(juce::String(card.description), juce::dontSendNotification);
  authorLabel.setText("by " + juce::String(card.author), juce::dontSendNotification);
}

void HARPProcessorEditor::buttonClicked(Button *button) {
  if (button == &processButton) {
    DBG("HARPProcessorEditor::buttonClicked button listener activated");
    
    auto model = mEditorView->getModel();
    mDocumentController->executeProcess(model);
  }

  else if (button == &loadModelButton) {
    DBG("HARPProcessorEditor::buttonClicked load model button listener activated");

    // collect input parameters for the model.
    std::map<std::string, std::any> params = {
      {"url", modelPathTextBox.getText().toStdString()},
    };

    resetUI();
    mDocumentController->executeLoad(params);

    // Model loading happens synchronously, so we can be sure that
    // the Editor View has the model card and UI attributes loaded
    setModelCard(mEditorView->getModel()->card());
    ctrlComponent.setModel(mEditorView->getModel());
    ctrlComponent.populateGui();
    resized();

  }
  else {
    DBG("a button was pressed, but we didn't do anything. ");
  }
}

// void HARPProcessorEditor::modelCardLoaded(const ModelCard& card) {
//   modelCardComponent.setModelCard(card); // addAndMakeVisible(modelCardComponent); 
// }

void HARPProcessorEditor::paint(Graphics &g) {
  g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));

  if (!isARAEditorView()) {
    g.setColour(Colours::white);
    g.setFont(15.0f);
    g.drawFittedText(
        "ARA host isn't detected. This plugin only supports ARA mode",
        getLocalBounds(), Justification::centred, 1);
  }
}


void HARPProcessorEditor::resized() {
    auto area = getLocalBounds();
    auto margin = 10;  // Adjusted margin value for top and bottom spacing

    auto docViewHeight = 100;  
    
    auto mainArea = area.removeFromTop(area.getHeight() - docViewHeight);
    auto documentViewArea = area;  // what remains is the 15% area for documentView

    // Row 1: Model Path TextBox and Load Model Button
    auto row1 = mainArea.removeFromTop(40);  // adjust height as needed
    modelPathTextBox.setBounds(row1.removeFromLeft(row1.getWidth() * 0.8f).reduced(margin));
    loadModelButton.setBounds(row1.reduced(margin));

    // Row 2: Name and Author Labels
    auto row2 = mainArea.removeFromTop(40);  // adjust height as needed
    nameLabel.setBounds(row2.removeFromLeft(row2.getWidth() / 2).reduced(margin));
    nameLabel.setFont(Font(20.0f, Font::bold));
    nameLabel.setColour(Label::textColourId, mHARPLookAndFeel.textHeaderColor);

    auto row25 = mainArea.removeFromTop(30);
    authorLabel.setBounds(row25.reduced(margin));
    authorLabel.setFont(Font(10.0f));


    // Row 3: Description Label
    auto row3 = mainArea.removeFromTop(80);  // adjust height as needed
    descriptionLabel.setBounds(row3.reduced(margin));

    // Row 5: Process Button (taken out in advance to preserve its height)
    auto row5Height = 25;  // adjust height as needed
    auto row5 = mainArea.removeFromBottom(row5Height);
    
    // Row 4: CtrlComponent (flexible height)
    auto row4 = mainArea;  // the remaining area is for row 4
    ctrlComponent.setBounds(row4.reduced(margin));

    // Assign bounds to processButton
    processButton.setBounds(row5.withSizeKeepingCentre(100, 20));  // centering the button in the row

    // DocumentView layout
    if (documentView != nullptr) {
        documentView->setBounds(documentViewArea);  // This should set the bounds correctly
    }
}
