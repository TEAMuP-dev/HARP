#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include <functional>

class ModelAuthorLabel : public juce::Component
{
public:
    ModelAuthorLabel(juce::Label& modelLabel, juce::Label& authorLabel, const juce::URL& url)
        : modelLabel(modelLabel), authorLabel(authorLabel), url(url)
    {
        addAndMakeVisible(modelLabel);
        addAndMakeVisible(authorLabel);

        // Create and configure the icon button
        iconDrawable = createIconDrawable();
        iconButton.setButtonText("");
        iconButton.setImages(iconDrawable.get());
        iconButton.onClick = [this, url] { url.launchInDefaultBrowser(); };
        addAndMakeVisible(iconButton);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto nameWidth =
            modelLabel.getFont().getStringWidthFloat(modelLabel.getText()) + 10; // Add some padding
        modelLabel.setBounds(area.removeFromLeft(static_cast<int>(nameWidth)));
        iconButton.setBounds(area.removeFromLeft(20)); // Adjust size as needed
        authorLabel.setBounds(area);
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
    juce::Label& modelLabel;
    juce::Label& authorLabel;
    juce::URL url;
    juce::DrawableButton iconButton { "iconButton", juce::DrawableButton::ImageFitted };
    std::unique_ptr<juce::Drawable> iconDrawable;

    std::unique_ptr<juce::Drawable> createIconDrawable()
    {
        auto drawable = std::make_unique<juce::DrawablePath>();
        juce::Path path;
        path.addEllipse(0, 0, 10, 10); // Example: a simple circle
        drawable->setPath(path);
        drawable->setFill(juce::Colours::blue); // Example: blue color
        return drawable;
    }
};