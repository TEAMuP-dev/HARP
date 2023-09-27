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

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include "../UI/DocumentView.h"

// #include "../UI/ModelCard.h"
#include "AudioProcessorImpl.h"
#include "EditorRenderer.h"
#include "PlaybackRenderer.h"
#include "EditorView.h"
#include "AudioModification.h"  
#include "../UI/LookAndFeel.h"

#include "DocumentControllerSpecialisation.h"

#include "CtrlComponent.h"
// #include "ModelCardComponent.h"

/**
 * @class HARPProcessorEditor
 * @brief Class responsible for managing the plugin's graphical interface.
 *
 * This class extends the base class AudioProcessorEditor and its ARA extension,
 * and implements Button::Listener, Slider::Listener, and ComboBox::Listener
 * interfaces.
 */
class HARPProcessorEditor : public AudioProcessorEditor,
                                  public AudioProcessorEditorARAExtension,
                                  public Button::Listener
                                  {
public:
  /**
   * @brief Constructor for HARPProcessorEditor.
   *
   * @param p Reference to HARPAudioProcessorImpl object.
   * @param er Pointer to EditorRenderer object.
   */
  explicit HARPProcessorEditor(HARPAudioProcessorImpl &p,
                                     EditorRenderer *er,
                                     PlaybackRenderer *pr,
                                     EditorView *ev);

  // destructor
  // ~HARPProcessorEditor() override;
  // Button listener method
  void buttonClicked(Button *button) override;

  // Paint method
  void paint(Graphics &g) override;

  // Resize method
  void resized() override;

private:
  void resetUI(){
    ctrlComponent.resetUI();
    // Also clear the model card components
    ModelCard empty;
    setModelCard(empty);

  }

  void setModelCard(const ModelCard& card);

private:

  HARPLookAndFeel mHARPLookAndFeel;


  unique_ptr<Component> documentView;
  juce::TextEditor modelPathTextBox;
  juce::TextButton loadModelButton;
  juce::TextButton processButton;

  CtrlComponent ctrlComponent;
  // model card
  juce::Label nameLabel, authorLabel, descriptionLabel, tagsLabel;

  EditorRenderer *mEditorRenderer;
  PlaybackRenderer *mPlaybackRenderer;
  EditorView *mEditorView;
  HARPDocumentControllerSpecialisation *mDocumentController;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HARPProcessorEditor)
};
