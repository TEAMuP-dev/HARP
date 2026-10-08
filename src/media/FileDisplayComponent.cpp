#include "FileDisplayComponent.h"

FileDisplayComponent::FileDisplayComponent() : FileDisplayComponent("File Track") {}

FileDisplayComponent::FileDisplayComponent(String name, bool req, bool fromDAW, DisplayMode mode)
    : MediaDisplayComponent(name, req, fromDAW, mode)
{
    downloadActiveMode = MultiButton::Mode { "Download",
                                             "Click to save the output file.",
                                             [this] { saveFileCallback(); },
                                             MultiButton::DrawingMode::IconOnly,
                                             Colours::lightblue,
                                             fontawesome::Save };
    downloadInactiveMode =
        MultiButton::Mode { "Download-Inactive", "No output file available.",
                            [this] {},           MultiButton::DrawingMode::IconOnly,
                            Colours::lightgrey,  fontawesome::Save };
    downloadButton.addMode(downloadActiveMode);
    downloadButton.addMode(downloadInactiveMode);
    downloadButton.setMode(downloadInactiveMode.displayLabel);
    downloadButton.setLookAndFeel(&noBorderLAF);

    addAndMakeVisible(downloadButton);
}

FileDisplayComponent::~FileDisplayComponent() { downloadButton.setLookAndFeel(nullptr); }

StringArray FileDisplayComponent::getSupportedExtensions()
{
    return {}; // Empty = accept all extensions
}

StringArray FileDisplayComponent::getInstanceExtensions()
{
    return instanceFileTypes; // Empty = accept all; populated from model metadata when provided
}

void FileDisplayComponent::setInstanceFileTypes(const std::vector<std::string>& types)
{
    instanceFileTypes.clear();

    for (const auto& t : types)
    {
        String ext(t);

        if (! ext.startsWithChar('.'))
            ext = "." + ext;

        instanceFileTypes.add(ext);
    }
}

double FileDisplayComponent::getTotalLengthInSecs() { return 0.0; }

void FileDisplayComponent::paint(Graphics& g)
{
    auto area = getLocalBounds();

    // Trimming past the available size yields a negative extent
    if (area.getHeight() <= labelHeight || area.getWidth() <= 0)
    {
        return;
    }

    auto body = area.withTrimmedTop(labelHeight).toFloat();
    auto bodyInt = body.toNearestInt();
    float r = 3.0f;

    auto bg = findColour(ComboBox::backgroundColourId);
    auto outline = findColour(ComboBox::outlineColourId);
    auto textCol = findColour(ComboBox::textColourId);

    // Draw label area
    g.setFont(Font(14.0f));
    g.setColour(textCol);
    g.drawText(
        getTrackName(), getLocalBounds().removeFromTop(labelHeight), Justification::centred, true);

    // Draw body background
    g.setColour(bg);
    g.fillRoundedRectangle(body.reduced(0.5f), r);
    g.setColour(outline);
    g.drawRoundedRectangle(body.reduced(0.5f), r, 1.0f);

    // Draw filename text (leaves space for the square button on the right)
    // The square button occupies the right of the body, but only if it fits
    auto textArea = bodyInt.withTrimmedRight(jmin(bodyInt.getHeight(), bodyInt.getWidth()))
                        .withTrimmedLeft(jmin(4, bodyInt.getWidth()));

    g.setFont(Font(13.0f));
    g.setColour(isFileLoaded() ? textCol.withAlpha(0.85f) : textCol.withAlpha(0.4f));
    g.drawText(isFileLoaded() ? getOriginalFilePath().getFileName() : "No output file",
               textArea,
               Justification::centredLeft,
               true);
}

void FileDisplayComponent::resized()
{
    auto area = getLocalBounds();

    if (area.getHeight() <= labelHeight || area.getWidth() <= 0)
    {
        downloadButton.setBounds({});
        return;
    }

    area = area.withTrimmedTop(labelHeight);

    int buttonSize = jmin(area.getHeight(), area.getWidth());
    downloadButton.setBounds(area.removeFromRight(buttonSize));
}

void FileDisplayComponent::loadMediaFile(const URL& /*filePath*/)
{
    downloadButton.setMode(downloadActiveMode.displayLabel);
    repaint();
}

void FileDisplayComponent::resetMedia()
{
    downloadButton.setMode(downloadInactiveMode.displayLabel);
    repaint();
}

void FileDisplayComponent::postLoadActions(const URL& /*filePath*/)
{
    // No extra action needed for generic files.
}
