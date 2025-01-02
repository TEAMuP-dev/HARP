#pragma once

#include "juce_gui_basics/juce_gui_basics.h"
#include <functional>
#include "../external/fontaudio/src/FontAudio.h"
#include "../external/fontawesome/src/FontAwesome.h"


class HoverableLabel : public juce::Label
{
public:
    HoverableLabel() 
    {
        // Make sure the label reacts to mouse events
        originalTextColor = findColour(juce::Label::textColourId);
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    // Override hitTest to allow hover only over text
    bool hitTest(int x, int y) override
    {
        // Get the text bounds
        auto textBounds = getTextBounds();

        // Check if the mouse coordinates are within the text bounds
        return textBounds.contains(x, y);
    }

    // Override mouse enter event
    void mouseEnter(const juce::MouseEvent& event) override
    {
        // Only change color if the mouse is over the text
        if (hitTest(event.x, event.y))
        {
            // Change text color to blue on hover
            setColour(juce::Label::textColourId, juce::Colours::coral);
            // setColour(juce::Label::backgroundColourId, juce::Colours::lightgrey); // Optional background color
            repaint(); // Request a repaint to reflect changes
            if (onHover) onHover(); // Call the hover callback if set
        }
        Label::mouseEnter(event); // Call base class
    }

    // Override mouse exit event
    void mouseExit(const juce::MouseEvent& event) override
    {
        // Reset label style back to the original color
        setColour(juce::Label::textColourId, originalTextColor); // Reset to original text color
        // setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack); // Reset background color
        if (onExit) onExit(); // Call the exit callback if set
        repaint(); // Request a repaint to reflect changes
        Label::mouseExit(event); // Call base class
    }

    // Override mouse down event (click event)
    void mouseDown(const juce::MouseEvent& event) override
    {
        if (hitTest(event.x, event.y) && onClick) // Check if clicking over the text
            onClick(); // Call the click callback if set
        Label::mouseDown(event); // Call base class
    }

    // Callback for hover event
    std::function<void()> onHover;
    std::function<void()> onExit;
    // Callback for click event
    std::function<void()> onClick;

private:

    juce::Rectangle<int> getTextBounds() const
    {
        auto font = getFont();
        auto textWidth = font.getStringWidth(getText());
        auto textHeight = font.getHeight();

        auto x_offset = (getBounds().getWidth() - textWidth) / 2;
        auto y_offset = (getBounds().getHeight() - textHeight) / 2;
        
        // Calculate the bounds considering the label's position
        return juce::Rectangle<int>(getX() + x_offset, getY() + y_offset, textWidth, textHeight);
    }

    juce::Colour originalTextColor;
};

class ModelAuthorLabel : public juce::Component
{
public:
    ModelAuthorLabel() //juce::Label& modelLabel, juce::Label& authorLabel, const juce::URL& url
        // :  url(url) //modelLabel(modelLabel), authorLabel(authorLabel),
    {
        // instructionBox = InstructionBox::getInstance();
        sharedFontAudio = std::make_shared<fontaudio::IconHelper>();
        sharedFontAwesome = std::make_shared<fontawesome::IconHelper>();
        

        // Set up hover and click callbacks
        modelLabel.onHover = [this] { 
            // Handle hover action
            instructionBox->setStatusMessage("Click to view the model's page");
                                 // + model->getGradioClient().getSpaceInfo().getModelSlashUser()
        };

        modelLabel.onExit = [this] {
            // Handle exit action
            instructionBox->clearStatusMessage();
        };

        modelLabel.onClick = [this] {
            // Handle click action
            url.launchInDefaultBrowser();
        };

        addAndMakeVisible(modelLabel);
        addAndMakeVisible(authorLabel);
        // iconButton.setButtonText("");
        // iconButton.onClick = [this] { url.launchInDefaultBrowser(); };
        // addAndMakeVisible(iconButton);
        
        // Create and configure the icon button
        // iconDrawable = createIconDrawable();
        // iconDrawable = createIconDrawableFromSVG("icons/external-link.svg");
        
        // iconButton.setImages(iconDrawable.get());
        // auto testIconName = fontawesome::FontAwesome_MousePointer;
        // auto testIcon = sharedFontAwesome->getIcon(testIconName, 20, juce::Colours::blue, 1.0f);
        // use createFromImageData(const void *data, size_t numBytes) to create a drawable from an image
        // iconDrawable->createFromImageData(testIcon.getPixelData(), 
        
        
    }

    void resized() override
    {
        auto area = getLocalBounds();
        modelLabel.setFont(juce::Font(22.0f, juce::Font::bold));
        auto nameWidth =
            modelLabel.getFont().getStringWidthFloat(modelLabel.getText()) + 10; // Add some padding
        modelLabel.setBounds(area.removeFromLeft(static_cast<int>(nameWidth)));
        // iconButton.setBounds(area.removeFromLeft(20)); // Adjust size as needed
        authorLabel.setBounds(area);
    }

    void paint(juce::Graphics& g) override
    {
        // g.fillAll(juce::Colours::white);
        // fontaudio::IconName offIcon =fontaudio::Bluetooth;
        // fontaudio::IconName onIcon = fontaudio::RoundswitchOn;
        // g.setFont(sharedFontAudio->getFont(getHeight() * 0.8f));
        // g.drawFittedText(offIcon, iconButton.getBounds(), Justification::centred, 1, 1);
        

        // fontawesome::IconName faicon = fontawesome::FontAwesome_ExternalLinkSquare;
        // sharedFontAwesome->drawCenterd(g, faicon, modelLabel.getHeight() * 1.0f, juce::Colours::red, iconButton.getBounds());
        // g.setFont(sharedFontAwesome->getFont(getHeight() * 0.8f));
        // g.drawFittedText(faicon, getLocalBounds(), Justification::centred, 1, 1);

        // just the default juce code in paint
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


    // juce::Label offIconLabel;
    juce::URL url;
    juce::DrawableButton iconButton { "iconButton", juce::DrawableButton::ImageFitted };
    std::unique_ptr<juce::Drawable> iconDrawable;

    std::shared_ptr<fontaudio::IconHelper> sharedFontAudio;
    std::shared_ptr<fontawesome::IconHelper> sharedFontAwesome;
    // std::shared_ptr<InstructionBox> instructionBox;
    // InstructionBox* instructionBox;
    juce::SharedResourcePointer<InstructionBox> instructionBox;

    std::unique_ptr<juce::Drawable> createIconDrawable()
    {
        auto drawable = std::make_unique<juce::DrawablePath>();
        juce::Path path;
        path.addEllipse(0, 0, 10, 10); // Example: a simple circle
        drawable->setPath(path);
        drawable->setFill(juce::Colours::grey); // Example: blue color
        return drawable;
    }
    
    std::unique_ptr<juce::Drawable> createIconDrawableFromSVG(const juce::String& svgFilePath)
    {
        juce::File aaa = juce::File::getCurrentWorkingDirectory().getChildFile (svgFilePath);
        juce::File svgFile(svgFilePath);
        juce::FileInputStream svgFileStream(svgFile);

        if (svgFileStream.openedOk())
        {
            auto svgXml = juce::XmlDocument::parse(svgFile);
            {
                return juce::Drawable::createFromSVG(*svgXml);
            }
        }

        return nullptr;
    }
};