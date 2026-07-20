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
    StringArray extensions;

    extensions.add(".txt");

    return extensions;
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
}

void TextDisplayComponent::resetMedia()
{
    textEditor.setText("No text file loaded.");
}

void TextDisplayComponent::postLoadActions(const URL& /*filePath*/) {}
