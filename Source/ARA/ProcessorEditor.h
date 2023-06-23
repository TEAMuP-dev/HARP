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

#pragma once

#include "../UI/DocumentView.h"
#include "../UI/ToolBarStyle.h"
#include "AudioProcessorImpl.h"
#include "EditorRenderer.h"

/**
 * @class TensorJuceProcessorEditor
 * @brief Class responsible for managing the plugin's graphical interface.
 *
 * This class extends the base class AudioProcessorEditor and its ARA extension,
 * and implements Button::Listener, Slider::Listener, and ComboBox::Listener
 * interfaces.
 */
class TensorJuceProcessorEditor : public AudioProcessorEditor,
                                  public AudioProcessorEditorARAExtension,
                                  public Button::Listener,
                                  public Slider::Listener,
                                  public ComboBox::Listener {
public:
  /**
   * @brief Constructor for TensorJuceProcessorEditor.
   *
   * @param p Reference to ARADemoPluginAudioProcessorImpl object.
   * @param er Pointer to EditorRenderer object.
   */
  explicit TensorJuceProcessorEditor(ARADemoPluginAudioProcessorImpl &p,
                                     EditorRenderer *er);

  // Button listener method
  void buttonClicked(Button *button) override;

  // Combo box listener method
  void comboBoxChanged(ComboBox *box) override;

  // Slider listener method
  void sliderValueChanged(Slider *slider) override;

  // Paint method
  void paint(Graphics &g) override;

  // Resize method
  void resized() override;

private:
  unique_ptr<Component> documentView;
  juce::TextButton testButton;

  juce::Slider temp1Dial;
  juce::Slider temp2Dial;
  juce::Slider stepsDial;
  juce::Slider phintDial;
  juce::Slider pwidthDial;

  juce::ComboBox modeBox;

  ToolbarSliderStyle toolbarSliderStyle;
  ButtonLookAndFeel buttonLookAndFeel;
  ComboBoxLookAndFeel comboBoxLookAndFeel;

  EditorRenderer *mEditorRenderer;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TensorJuceProcessorEditor)
};
