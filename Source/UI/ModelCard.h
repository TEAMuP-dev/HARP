/**
 * @class ModelCardComponent
 * @brief graphical interface for model metadata
 */

#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include "CustomComponents.h"

struct ModelCard {
  int sampleRate;
  std::string name;
  std::string description;
  std::string author;
  std::vector<std::string> tags;
};

class ModelCardListener
{
public:
    virtual void modelCardLoaded(const ModelCard& card) = 0; // Called when a model card has been loaded
};

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

    void clear() {
        // Clear the text for the labels
        nameLabel.setText("", juce::dontSendNotification);
        descriptionLabel.setText("", juce::dontSendNotification);
        authorLabel.setText("", juce::dontSendNotification);
        sampleRateLabel.setText("", juce::dontSendNotification);
        tagsLabel.setText("", juce::dontSendNotification);
        
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