#include "TextDisplayComponent.h"

TextDisplayComponent::TextDisplayComponent() : TextDisplayComponent("Text Track") {}

TextDisplayComponent::TextDisplayComponent(String name, bool req, bool fromDAW, DisplayMode mode)
    : MediaDisplayComponent(name, req, fromDAW, mode)
{
    textEditor.setMultiLine(true);
    textEditor.setReadOnly(true);
    textEditor.setScrollbarsShown(true);
    textEditor.setCaretVisible(false);
    textEditor.setPopupMenuEnabled(false);
    textEditor.setText("No text file loaded.");
    contentComponent.addAndMakeVisible(textEditor);
}

StringArray TextDisplayComponent::getSupportedExtensions()
{
    return { ".txt" };
}

StringArray TextDisplayComponent::getInstanceExtensions()
{
    return getSupportedExtensions();
}

double TextDisplayComponent::getTotalLengthInSecs() { return 0.0; }

void TextDisplayComponent::paint(Graphics& g)
{
    MediaDisplayComponent::paint(g);
}

void TextDisplayComponent::resized()
{
    MediaDisplayComponent::resized();
    textEditor.setBounds(contentComponent.getLocalBounds());
}

void TextDisplayComponent::loadMediaFile(const URL& filePath)
{
    File file = filePath.getLocalFile();
    textEditor.setText(file.loadFileAsString());
    repaint();
}

void TextDisplayComponent::resetMedia()
{
    textEditor.setText("No text file loaded.");
    repaint();
}

void TextDisplayComponent::postLoadActions(const URL& /*filePath*/)
{
    repaint();
}
