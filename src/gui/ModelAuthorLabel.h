#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include <functional>
#include "../external/fontaudio/src/FontAudio.h"
#include "../external/fontawesome/src/FontAwesome.h"
#include "CustomPathDialog.h"
#include "StatusComponent.h"
#include "HoverableLabel.h"

class ModelAuthorLabel : public juce::Component
{
public:
    ModelAuthorLabel()
    {
        modelLabel.onHover = [this] { 
            instructionBox->setStatusMessage("Click to view the model's page");
        };

        modelLabel.onExit = [this] {
            instructionBox->clearStatusMessage();
        };

        modelLabel.onClick = [this] {
            url.launchInDefaultBrowser();
        };
        modelLabel.setHoverColour(juce::Colours::coral);

        addAndMakeVisible(modelLabel);
        addAndMakeVisible(authorLabel);

        
    }

    void resized() override
    {
        auto area = getLocalBounds();
        modelLabel.setFont(juce::Font(22.0f, juce::Font::bold));
        auto nameWidth =
            modelLabel.getFont().getStringWidthFloat(modelLabel.getText()) + 10;
        modelLabel.setBounds(area.removeFromLeft(static_cast<int>(nameWidth)));
        authorLabel.setBounds(area.translated(0, 3));

    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void setModelText(const juce::String& name)
    {
        modelLabel.setText(name, juce::dontSendNotification);
    }
    void setAuthorText(const juce::String& name)
    {
        authorLabel.setText(name, juce::dontSendNotification);
    }
    void setURL(const juce::URL& newURL)
    {
        url = newURL;
    }

private:
    HoverableLabel modelLabel;
    juce::Label authorLabel;
    juce::URL url;
    juce::SharedResourcePointer<InstructionBox> instructionBox;
};