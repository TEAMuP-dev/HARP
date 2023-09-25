#include "juce_gui_basics/juce_gui_basics.h"
#include "../DeepLearning/Model.h"

class ModelCardComponent : public juce::Component
{
public:
    ModelCardComponent() {
        initialiseLabels();
    }

    void setModelCard(const ModelCard& card) {
        // Set the text for the labels
        nameLabelValue.setText(juce::String(card.name), juce::dontSendNotification);
        descriptionLabelValue.setText(juce::String(card.description), juce::dontSendNotification);
        authorLabelValue.setText(juce::String(card.author), juce::dontSendNotification);
        sampleRateLabelValue.setText(card.sampleRate == 0 ? "" : juce::String(card.sampleRate), juce::dontSendNotification);

        juce::String tagsText;
        for (const auto& tag : card.tags) {
            tagsText += juce::String(tag) + ", ";
        }
        tagsText = tagsText.dropLastCharacters(2);  // Remove trailing comma and space
        tagsLabelValue.setText(tagsText, juce::dontSendNotification);

        repaint();
    }

    void clear() {
        // Clear the text for the labels
        nameLabelValue.setText("", juce::dontSendNotification);
        descriptionLabelValue.setText("", juce::dontSendNotification);
        authorLabelValue.setText("", juce::dontSendNotification);
        sampleRateLabelValue.setText("", juce::dontSendNotification);
        tagsLabelValue.setText("", juce::dontSendNotification);

        repaint();
    }

    void resized() override {
        const int labelHeight = 20;
        const int margin = 2;
        const int startingY = 10;
        
        int yPosition = startingY;
        
        nameLabel.setBounds(10, yPosition, getWidth() - 20, labelHeight);
        nameLabelValue.setBounds(10, yPosition += labelHeight, getWidth() - 20, labelHeight);
        
        descriptionLabel.setBounds(10, yPosition += (margin + labelHeight), getWidth() - 20, labelHeight);
        descriptionLabelValue.setBounds(10, yPosition += labelHeight, getWidth() - 20, labelHeight);
        
        authorLabel.setBounds(10, yPosition += (margin + labelHeight), getWidth() - 20, labelHeight);
        authorLabelValue.setBounds(10, yPosition += labelHeight, getWidth() - 20, labelHeight);
        
        sampleRateLabel.setBounds(10, yPosition += (margin + labelHeight), getWidth() - 20, labelHeight);
        sampleRateLabelValue.setBounds(10, yPosition += labelHeight, getWidth() - 20, labelHeight);
        
        tagsLabel.setBounds(10, yPosition += (margin + labelHeight), getWidth() - 20, labelHeight);
        tagsLabelValue.setBounds(10, yPosition += labelHeight, getWidth() - 20, labelHeight);
    }

private:
    void initialiseLabels() {
        // Initialize the labels with bold text for names
        nameLabel.setText("Name:", juce::dontSendNotification);
        descriptionLabel.setText("Description:", juce::dontSendNotification);
        authorLabel.setText("Author:", juce::dontSendNotification);
        sampleRateLabel.setText("Sample Rate:", juce::dontSendNotification);
        tagsLabel.setText("Tags:", juce::dontSendNotification);

        for (auto* label : { &nameLabel, &descriptionLabel, &authorLabel, &sampleRateLabel, &tagsLabel }) {
            label->setFont(juce::Font(14.0f, juce::Font::bold));
            addAndMakeVisible(label);
        }

        for (auto* label : { &nameLabelValue, &descriptionLabelValue, &authorLabelValue, &sampleRateLabelValue, &tagsLabelValue }) {
            label->setFont(juce::Font(14.0f));
            addAndMakeVisible(label);
        }
    }

    juce::Label nameLabel, descriptionLabel, authorLabel, sampleRateLabel, tagsLabel;
    juce::Label nameLabelValue, descriptionLabelValue, authorLabelValue, sampleRateLabelValue, tagsLabelValue;
};
