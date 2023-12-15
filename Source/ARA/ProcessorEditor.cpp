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
    mDocumentController->addLoadListener(this);
    mDocumentController->addProcessListener(this);
    documentView = std::make_unique<DocumentView>(*mEditorView, ap.playHeadState);
  }
  else {
    DBG("FATAL HARPProcessorEditor::HARPProcessorEditor: mEditorView is null");
    return;
  }

  if (documentView != nullptr) {
    addAndMakeVisible(documentView.get());
  }
  juce::LookAndFeel::setDefaultLookAndFeel (&mHARPLookAndFeel);

  // TODO: what happens if the model is nullptr rn?
  auto model = mEditorView->getModel();
  if (model == nullptr) {
    DBG("FATAL HARPProcessorEditor::HARPProcessorEditor: model is null");
    jassertfalse;
    return;
  }

  // initialize load and process buttons
  processButton.setButtonText("process");
  processButton.addListener(this);
  model->ready() ? processButton.setEnabled(true)
                : processButton.setEnabled(false);
  addAndMakeVisible(processButton);

  cancelButton.setButtonText("cancel");
  cancelButton.addListener(this);
  cancelButton.setEnabled(false);
  addAndMakeVisible(cancelButton);

  loadModelButton.setButtonText("load");
  loadModelButton.addListener(this);
  addAndMakeVisible(loadModelButton);

  std::string currentStatus = model->getStatus();
  if (currentStatus == "Status.LOADED" || currentStatus == "Status.FINISHED") {
    processButton.setEnabled(true);
    processButton.setButtonText("process");
  } else if (currentStatus == "Status.PROCESSING" || currentStatus == "Status.STARTING" || currentStatus == "Status.SENDING") {
    cancelButton.setEnabled(true);
    processButton.setButtonText("processing " + juce::String(model->card().name) + "...");
  }

  // status label
  statusLabel.setText(currentStatus, juce::dontSendNotification);
  addAndMakeVisible(statusLabel);

  // add a status timer to update the status label periodically
  mModelStatusTimer = std::make_unique<ModelStatusTimer>(model);
  mModelStatusTimer->addChangeListener(this);
  mModelStatusTimer->startTimer(100);  // 100 ms interval

  // model path textbox
  modelPathTextBox.setMultiLine(false);
  modelPathTextBox.setReturnKeyStartsNewLine(false);
  modelPathTextBox.setReadOnly(false);
  modelPathTextBox.setScrollbarsShown(false);
  modelPathTextBox.setCaretVisible(true);
  modelPathTextBox.setTextToShowWhenEmpty("path to a gradio endpoint", juce::Colour::greyLevel(0.5f));  // Default text
  modelPathTextBox.onReturnKey = [this] { loadModelButton.triggerClick(); };
  addAndMakeVisible(modelPathTextBox);

  // glossary label
  glossaryLabel.setText("To view an index of available HARP-compatible models, please see our ", juce::NotificationType::dontSendNotification);
  glossaryLabel.setJustificationType(juce::Justification::centredRight);
  addAndMakeVisible(glossaryLabel);

  // glossary link
  glossaryButton.setButtonText("Model Glossary");
  glossaryButton.setURL(juce::URL("https://github.com/audacitorch/HARP#available-models"));
  //glossaryButton.setJustificationType(juce::Justification::centredLeft);
  glossaryButton.addListener(this);
  addAndMakeVisible(glossaryButton);

  if (model->ready()) {
    modelPathTextBox.setText(model->space_url());  // Default text
  } else {
    modelPathTextBox.setText("path to a gradio endpoint");  // Default text
  }
  addAndMakeVisible(modelPathTextBox);

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
  // set the author label text to "by {author}" only if {author} isn't empty
  card.author.empty() ?
    authorLabel.setText("", juce::dontSendNotification) :
    authorLabel.setText("by " + juce::String(card.author), juce::dontSendNotification);
}

void HARPProcessorEditor::buttonClicked(Button *button) {
  if (button == &processButton) {
    DBG("HARPProcessorEditor::buttonClicked button listener activated");

    auto model = mEditorView->getModel();
    mDocumentController->executeProcess(model);
    // set the button text to "processing {model.card().name}"
    processButton.setButtonText("processing " + juce::String(model->card().name) + "...");
    processButton.setEnabled(false);

    // enable the cancel button
    cancelButton.setEnabled(true);
  }

  else if (button == &loadModelButton) {
    DBG("HARPProcessorEditor::buttonClicked load model button listener activated");

    // collect input parameters for the model.
    std::map<std::string, std::any> params = {
      {"url", modelPathTextBox.getText().toStdString()},
    };

    resetUI();
    // loading happens asynchronously.
    // the document controller trigger a change listener callback, which will update the UI
    mDocumentController->executeLoad(params);

    // disable the load button until the model is loaded
    loadModelButton.setEnabled(false);
    loadModelButton.setButtonText("loading...");

    // disable the process button until the model is loaded
    processButton.setEnabled(false);

    // set the descriptionLabel to "loading {url}..."
    // TODO: we need to get rid of the params map, and just pass the url around instead
    // since it looks like we're sticking to webmodels.
    juce::String url = juce::String(std::any_cast<std::string>(params.at("url")));
    descriptionLabel.setText("loading " + url + "...\n if this takes a while, check if the huggingface space is sleeping by visiting \n " + "huggingface.co/spaces/" + url + "\n Once the huggingface space is awake, try again." , juce::dontSendNotification);

    // TODO: here, we should also reset the highlighting of the playback regions to the default color
  }
  else if (button == &cancelButton) {
    DBG("HARPProcessorEditor::buttonClicked cancel button listener activated");
    mDocumentController->getModel()->cancel();
  }
  else {
    DBG("a button was pressed, but we didn't do anything. ");
  }
}

void HARPProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster *source) {

  if (mDocumentController->isLoadBroadcaster(source)) {
    // Model loading happens synchronously, so we can be sure that
    // the Editor View has the model card and UI attributes loaded
    DBG("Setting up model card, CtrlComponent, resizing.");
    setModelCard(mEditorView->getModel()->card());
    ctrlComponent.setModel(mEditorView->getModel());
    ctrlComponent.populateGui();
    resized();

    // now, we can enable the buttons
    processButton.setEnabled(true);
    loadModelButton.setEnabled(true);
    loadModelButton.setButtonText("load");
  }
  else if (mDocumentController->isProcessBroadcaster(source)) {
    // now, we can enable the process button
    processButton.setButtonText("process");
    processButton.setEnabled(true);
    cancelButton.setEnabled(false);
    mEditorView->triggerRepaint();
  }
  else if (source == mModelStatusTimer.get()) {
    // update the status label
    DBG("HARPProcessorEditor::changeListenerCallback: updating status label");
    statusLabel.setText(mEditorView->getModel()->getStatus(), juce::dontSendNotification);
  }
  else {
    DBG("HARPProcessorEditor::changeListenerCallback: unhandled change broadcaster");
  }

}


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

    // Row 2: Glossary Label and Hyperlink
    auto row2 = mainArea.removeFromTop(30);  // adjust height as needed
    glossaryLabel.setBounds(row2.removeFromLeft(row2.getWidth() * 0.8f).reduced(margin));
    glossaryButton.setBounds(row2.reduced(margin));
    glossaryLabel.setFont(Font(11.0f));
    glossaryButton.setFont(Font(11.0f), false, juce::Justification::centredLeft);

    // Row 3: Name and Author Labels
    auto row3a = mainArea.removeFromTop(40);  // adjust height as needed
    nameLabel.setBounds(row3a.removeFromLeft(row3a.getWidth() / 2).reduced(margin));
    nameLabel.setFont(Font(20.0f, Font::bold));
    nameLabel.setColour(Label::textColourId, mHARPLookAndFeel.textHeaderColor);

    auto row3b = mainArea.removeFromTop(30);
    authorLabel.setBounds(row3b.reduced(margin));
    authorLabel.setFont(Font(10.0f));


    // Row 4: Description Label
    auto row4 = mainArea.removeFromTop(80);  // adjust height as needed
    descriptionLabel.setBounds(row4.reduced(margin));

    // Row 6: Process Button (taken out in advance to preserve its height)
    auto row6Height = 25;  // adjust height as needed
    auto row6 = mainArea.removeFromBottom(row6Height);

    // Row 5: CtrlComponent (flexible height)
    auto row5 = mainArea;  // the remaining area is for row 4
    ctrlComponent.setBounds(row5.reduced(margin));

    // Assign bounds to processButton
    processButton.setBounds(row6.withSizeKeepingCentre(100, 20));  // centering the button in the row

    // place the cancel button to the right of the process button (justified right)
    cancelButton.setBounds(processButton.getBounds().translated(110, 0));

    // place the status label to the left of the process button (justified left)
    statusLabel.setBounds(processButton.getBounds().translated(-200, 0));

    // DocumentView layout
    if (documentView != nullptr) {
        documentView->setBounds(documentViewArea);  // This should set the bounds correctly
    }
}
