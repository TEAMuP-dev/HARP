#include "MultiButton.h"
#include "CustomPathDialog.h"
// MultiButton::MultiButton(const Mode& mode1, const Mode& mode2)
// {
//     addMode(mode1);
//     addMode(mode2);
//     setMode(mode1.label);
// }
MultiButton::MultiButton(const juce::String& buttonName) : juce::TextButton(buttonName)
{
    // Optionally set default properties
    setToggleState(false, juce::dontSendNotification);
    fontawesomeHelper = std::make_shared<fontawesome::IconHelper>();
    fontaudioHelper = std::make_shared<fontaudio::IconHelper>();
}

// MultiButton::MultiButton()
// {
//     setToggleState(false, juce::dontSendNotification);
//     fontawesomeHelper = std::make_shared<fontawesome::IconHelper>();
// }

void MultiButton::addMode(const Mode& mode)
{
    // Check if the mode.label already exists
    if (modes.find(mode.label) != modes.end())
    {
        // If it does, print a warning
        DBG("Mode with label " + mode.label + " already exists. Overwriting.");
    }
    modes[mode.label] = mode;
}

void MultiButton::setMode(const juce::String& modeName)
{
    if (modes.find(modeName) != modes.end() && currentMode != modeName)
    {
        currentMode = modeName;
        setButtonText(modes[currentMode].label);
        // setColour(juce::TextButton::buttonColourId, modes[currentMode].color);
        onClick = modes[currentMode].callback;
        repaint();
    }
}

void MultiButton::mouseEnter(const juce::MouseEvent& event)
{
    // First call the base class method
    juce::TextButton::mouseEnter(event);
    instructionBox->setStatusMessage(modes[currentMode].instructionMessage);
    if (onMouseEnter)
    {
        onMouseEnter();
    }
}

void MultiButton::mouseExit(const juce::MouseEvent& event)
{
    // First call the base class method
    juce::TextButton::mouseExit(event);
    instructionBox->clearStatusMessage();
    if (onMouseExit)
    {
        onMouseExit();
    }
}

juce::String MultiButton::getModeName() { return currentMode; }

void MultiButton::paintButton(juce::Graphics& g,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown)
{
    // juce::TextButton::paintButton(g, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    auto& lf = getLookAndFeel();

    lf.drawButtonBackground(g,
                            *this,
                            findColour(getToggleState() ? buttonOnColourId : buttonColourId),
                            shouldDrawButtonAsHighlighted,
                            shouldDrawButtonAsDown);

    if (modes[currentMode].drawingMode == DrawingMode::TextOnly)
    {
        // Code to draw the button text
        Font font(lf.getTextButtonFont(*this, getHeight()));
        g.setFont(font);
        g.setColour(
            findColour(getToggleState() ? TextButton::textColourOnId : TextButton::textColourOffId)
                .withMultipliedAlpha(isEnabled() ? 1.0f : 0.5f));

        const int yIndent = jmin(4, proportionOfHeight(0.3f));
        const int cornerSize = jmin(getHeight(), getWidth()) / 2;

        const int fontHeight = roundToInt(font.getHeight() * 0.6f);
        const int leftIndent = jmin(fontHeight, 2 + cornerSize / (isConnectedOnLeft() ? 4 : 2));
        const int rightIndent = jmin(fontHeight, 2 + cornerSize / (isConnectedOnRight() ? 4 : 2));
        const int textWidth = getWidth() - leftIndent - rightIndent;

        if (textWidth > 0)
            g.drawFittedText(getButtonText(),
                            leftIndent,
                            yIndent,
                            textWidth,
                            getHeight() - yIndent * 2,
                            Justification::centred,
                            2);
    }
    else if (modes[currentMode].drawingMode == DrawingMode::IconOnly)
    {
        auto textArea = getLocalBounds();//.reduced(10); // Add some padding
        // auto icon = fontawesomeHelper->getIcon(currentIconName, textArea.getHeight(), currentColor, 1.0f);
        juce::String currentIconName;
        auto currentColor = modes[currentMode].color;
        auto size = jmin(getWidth(), getHeight());
        if (modes[currentMode].iconType == IconType::FontAwesome)
        {
            currentIconName = modes[currentMode].awesomeIcon;
            auto icon = fontawesomeHelper->getIcon(currentIconName, size, currentColor, 1.0f);
            fontawesomeHelper->drawCenterdAt(g, icon, getLocalBounds(), 1.0f);
        }
        else if (modes[currentMode].iconType == IconType::FontAudio)
        {
            currentIconName = modes[currentMode].audioIcon;
            auto icon = fontaudioHelper->getIcon(currentIconName, size, currentColor, 1.0f);
            fontaudioHelper->drawCenterdAt(g, icon, getLocalBounds(), 1.0f);
        }
        // auto icon = fontawesomeHelper->getIcon(currentIconName, size, currentColor, 1.0f);
        // fontawesomeHelper->drawCenterdAt(g, icon, getLocalBounds(), 1.0f);
        // auto icon = fontaudioHelper->getIcon(currentIconName, size, currentColor, 1.0f);
        // fontaudioHelper->drawCenterdAt(g, icon, getLocalBounds(), 1.0f);
        
    }
    // int shouldDrawButtonAsHighlightedInt = shouldDrawButtonAsHighlighted ? 1 : 0;
    // int shouldDrawButtonAsDownInt = shouldDrawButtonAsDown ? 1 : 0;
    // DBG("shouldDrawButtonAsHighlighted: " + juce::String(shouldDrawButtonAsHighlightedInt));
    // DBG("shouldDrawButtonAsDown: " + juce::String(shouldDrawButtonAsDownInt));
}

void MultiButton::paint(juce::Graphics& g)
{
    juce::TextButton::paint(g); // Call the base class paint method
}

void MultiButton::resized() 
{
    if (modes[currentMode].drawingMode == DrawingMode::IconOnly)
    {
        auto size = jmin(getWidth(), getHeight());
        setSize(size, size);
    }
}