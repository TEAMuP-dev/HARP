#include "MultiButton.h"

MultiButton::MultiButton(const String& buttonName) : TextButton(buttonName)
{
    // Set default properties

    setToggleState(false, dontSendNotification);

    fontawesomeHelper = std::make_shared<fontawesome::IconHelper>();
    fontaudioHelper = std::make_shared<fontaudio::IconHelper>();
}

void MultiButton::paintButton(Graphics& g,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown)
{
    LookAndFeel& lf = getLookAndFeel();

    lf.drawButtonBackground(g,
                            *this,
                            findColour(getToggleState() ? buttonOnColourId : buttonColourId),
                            shouldDrawButtonAsHighlighted,
                            shouldDrawButtonAsDown);

    if (modes[currentModeKey].drawingMode == DrawingMode::TextOnly)
    {
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
        {
            g.drawFittedText(getButtonText(),
                             leftIndent,
                             yIndent,
                             textWidth,
                             getHeight() - yIndent * 2,
                             Justification::centred,
                             2);
        }
    }

    if (modes[currentModeKey].drawingMode == DrawingMode::IconOnly)
    {
        String currentIconKey;

        int size = getIconSize();

        Colour modeColor = modes[currentModeKey].iconColor;

        if (modes[currentModeKey].iconType == IconType::FontAwesome)
        {
            currentIconKey = modes[currentModeKey].awesomeIcon;

            auto icon = fontawesomeHelper->getIcon(currentIconKey, size, modeColor, 1.0f);

            fontawesomeHelper->drawCenterdAt(g, icon, getLocalBounds(), 1.0f);
        }
        else if (modes[currentModeKey].iconType == IconType::FontAudio)
        {
            currentIconKey = modes[currentModeKey].audioIcon;

            auto icon = fontaudioHelper->getIcon(currentIconKey, size, modeColor, 1.0f);

            fontaudioHelper->drawCenterdAt(g, icon, getLocalBounds(), 1.0f);
        }
    }
}

void MultiButton::resized()
{
    if (modes[currentModeKey].drawingMode == DrawingMode::IconOnly)
    {
        int size = getIconSize();

        setSize(size, size);
    }
}

void MultiButton::addMode(const Mode& mode)
{
    // Check if mode.key already exists
    if (modes.find(mode.displayLabel) != modes.end())
    {
        DBG_AND_LOG("MultiButton::addMode: Mode with key \"" + mode.displayLabel
                    + "\" already exists and is being overwritten.");
    }

    // Add new mode to dictionary
    modes[mode.displayLabel] = mode;
}

void MultiButton::setMode(const String& modeKey)
{
    if (modes.find(modeKey) != modes.end() && currentModeKey != modeKey)
    {
        currentModeKey = modeKey;

        setButtonText(modes[currentModeKey].displayLabel);

        onClick = modes[currentModeKey].callback;

        if (isMouseOver())
        {
            updateInstructions();
        }

        repaint();
    }
}

void MultiButton::updateInstructions()
{
    String instructions = modes[currentModeKey].instructions;

    if (instructionsMessage != nullptr && instructions.isNotEmpty())
    {
        instructionsMessage->setMessage(instructions);
    }
}

void MultiButton::mouseEnter(const MouseEvent& event)
{
    // Invoke base class method
    TextButton::mouseEnter(event);

    updateInstructions();

    if (onMouseEnter)
    {
        onMouseEnter();
    }
}

void MultiButton::mouseExit(const MouseEvent& event)
{
    // Invoke base class method
    TextButton::mouseExit(event);

    if (instructionsMessage != nullptr)
    {
        instructionsMessage->clearMessage();
    }

    if (onMouseExit)
    {
        onMouseExit();
    }
}
