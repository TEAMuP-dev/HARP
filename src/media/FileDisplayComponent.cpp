#include "FileDisplayComponent.h"

FileDisplayComponent::FileDisplayComponent()
    : FileDisplayComponent("File Track")
{
}

FileDisplayComponent::FileDisplayComponent(String name, bool req, bool fromDAW, DisplayMode mode)
    : MediaDisplayComponent(name, req, fromDAW, mode)
{
}

FileDisplayComponent::~FileDisplayComponent()
{
}

StringArray FileDisplayComponent::getSupportedExtensions()
{
    return {
        ".nam",
        ".txt",
        ".csv",
        ".json",
        ".pth",
        ".pt",
        ".onnx"
    };
}

StringArray FileDisplayComponent::getInstanceExtensions()
{
    return FileDisplayComponent::getSupportedExtensions();
}

double FileDisplayComponent::getTotalLengthInSecs()
{
    return 0.0;
}

void FileDisplayComponent::loadMediaFile(const URL& filePath)
{
    setTrackName(filePath.getFileName());
    postLoadActions(filePath);
    repaint();
}

void FileDisplayComponent::resetMedia()
{
    repaint();
}

void FileDisplayComponent::postLoadActions(const URL& /*filePath*/)
{
    // No extra action needed for generic files.
}