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

#include "juce_gui_basics/juce_gui_basics.h"
#include "../UI/DocumentView.h"
#include "../UI/ToolBarStyle.h"
#include "../UI/CustomComponents.h"
#include "AudioProcessorImpl.h"
#include "EditorRenderer.h"
#include "AudioModification.h"  
// #include "../DeepLearning/TorchModel.h"
#include <torch/script.h>
#include <torch/torch.h>
#include "../DeepLearning/TorchModel.h" // needed for ModelCardListener


/**
 * @class ModelCardComponent
 * @brief graphical interface for model metadata
 */
class ModelCardComponent : public juce::Component {
public:
    ModelCardComponent() {
        // Initialize the labels
        addAndMakeVisible(nameLabel);
        addAndMakeVisible(descriptionLabel);
        addAndMakeVisible(authorLabel);
        addAndMakeVisible(sampleRateLabel);
        addAndMakeVisible(tagsLabel);
    }

    void setModelCard(const ModelCard& card) {
        // Set the text for the labels
        nameLabel.setText("Name: " + juce::String(card.name), juce::dontSendNotification);
        descriptionLabel.setText("Description: " + juce::String(card.description), juce::dontSendNotification);
        authorLabel.setText("Author: " + juce::String(card.author), juce::dontSendNotification);
        sampleRateLabel.setText("Sample Rate: " + juce::String(card.sampleRate), juce::dontSendNotification);
        
        juce::String tagsText = "Tags: ";
        for (const auto& tag : card.tags) {
            tagsText += juce::String(tag) + ", ";
        }
        tagsText = tagsText.dropLastCharacters(2);  // Remove trailing comma and space
        tagsLabel.setText(tagsText, juce::dontSendNotification);
        
        // Repaint the component to update the display
        repaint();
    }

    void resized() override {
        // Create a FlexBox instance
        juce::FlexBox flexBox;
        flexBox.flexDirection = juce::FlexBox::Direction::column;
        flexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;
        flexBox.alignItems = juce::FlexBox::AlignItems::stretch;

        // Add each label to the FlexBox with uniform flex
        const float flexValue = 1.0f;
        flexBox.items.add(juce::FlexItem(nameLabel).withFlex(flexValue));
        flexBox.items.add(juce::FlexItem(descriptionLabel).withFlex(flexValue));
        flexBox.items.add(juce::FlexItem(authorLabel).withFlex(flexValue));
        flexBox.items.add(juce::FlexItem(sampleRateLabel).withFlex(flexValue));
        flexBox.items.add(juce::FlexItem(tagsLabel).withFlex(flexValue));

        // Perform layout in the component's local bounds
        flexBox.performLayout(getLocalBounds());
    }

private:
    juce::Label nameLabel;
    juce::Label descriptionLabel;
    juce::Label authorLabel;
    juce::Label sampleRateLabel;
    juce::Label tagsLabel;
};


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
                                  public ComboBox::Listener,
                                  public TextEditor::Listener,
                                  public ChangeListener,
                                  public ModelCardListener // TODO from hf. Check if I need that
                                   {
public:
  /**
   * @brief Constructor for TensorJuceProcessorEditor.
   *
   * @param p Reference to TensorJuceAudioProcessorImpl object.
   * @param er Pointer to EditorRenderer object.
   */
  explicit TensorJuceProcessorEditor(TensorJuceAudioProcessorImpl &p,
                                     EditorRenderer *er);

  // destructor
  // ~TensorJuceProcessorEditor() override;
  // Button listener method
  void buttonClicked(Button *button) override;

  // Combo box listener method
  void comboBoxChanged(ComboBox *box) override;

  // Slider listener method
  void sliderValueChanged(Slider *slider) override;

  // Model card listener method
  void modelCardLoaded(const ModelCard& card) override;

  // Paint method
  void paint(Graphics &g) override;

  // Resize method
  void resized() override;

  // Change listener method
  void changeListenerCallback(ChangeBroadcaster *source) override;

private:

  void InitGenericDial(
    juce::Slider &dial,
    const juce::String& valueSuffix, 
    const juce::Range<double> range, 
    double step_size,
    float value
  );

private:
  unique_ptr<Component> documentView;
  juce::TextButton loadModelButton;
  juce::TextButton processButton;

  // An vector of unique pointers to sliders
  std::vector<std::unique_ptr<juce::Slider>> continuousCtrls;
  std::vector<std::unique_ptr<juce::ToggleButton>> binaryCtrls;
  std::vector<std::unique_ptr<juce::ComboBox>> optionCtrls;
  std::vector<std::unique_ptr<TitledTextBox>> textCtrls;

  // std::shared_ptr<TorchWave2Wave> model;
  ModelCardComponent modelCardComponent;

  std::unique_ptr<juce::FileChooser> modelPathChooser {nullptr};

  ToolbarSliderStyle toolbarSliderStyle;
  ButtonLookAndFeel buttonLookAndFeel;
  ComboBoxLookAndFeel comboBoxLookAndFeel;

  EditorRenderer *mEditorRenderer;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TensorJuceProcessorEditor)
};
