/**
 * @file FileChooserWithLabel.h
 * @brief Custom file chooser component with label.
 * @author derekllanes, cwitkowitz
 */

#pragma once

#include <string>
#include <vector>

#include "ControlComponent.h"
#include "MultiButton.h"

using namespace juce;

class FileChooserWithLabel : public ControlComponent, public FileDragAndDropTarget
{
public:
    ~FileChooserWithLabel() { actionButton.setLookAndFeel(nullptr); }

    FileChooserWithLabel(const String& labelText = {})
    {
        label.setText(labelText, dontSendNotification);
        label.setJustificationType(Justification::centred);
        addAndMakeVisible(label);

        initializeButton();
        addAndMakeVisible(actionButton);
    }

    void resized() override
    {
        auto area = getLocalBounds();

        label.setBounds(area.removeFromTop(jmin(labelHeight, area.getHeight())));

        if (! required)
            area.removeFromTop(jmin(bannerHeight, area.getHeight()));

        if (area.isEmpty())
        {
            actionButton.setBounds({});
            return;
        }

        int buttonSize = jmin(area.getHeight(), area.getWidth());
        actionButton.setBounds(area.removeFromRight(buttonSize));
    }

    void paint(Graphics& g) override
    {
        auto area = getLocalBounds();

        // Drawing into a negative-sized rectangle trips a JUCE assertion
        if (area.getHeight() <= labelHeight || area.getWidth() <= 0)
        {
            return;
        }

        area.removeFromTop(labelHeight);

        if (! required)
        {
            auto bannerRect = area.removeFromTop(bannerHeight).toFloat();
            g.setColour(Colour::fromRGB(90, 105, 105));
            g.fillRect(bannerRect);
            g.setColour(Colours::white);
            g.setFont(Font(12.0f));
            g.drawText("OPTIONAL", bannerRect, Justification::centred, false);
        }

        auto body = area.toFloat();
        auto bodyInt = area;
        float r = 3.0f;

        auto bg = findColour(ComboBox::backgroundColourId);
        auto outline = findColour(ComboBox::outlineColourId);
        auto textCol = findColour(ComboBox::textColourId);

        g.setColour(bg);
        g.fillRoundedRectangle(body.reduced(0.5f), r);
        g.setColour(outline);
        g.drawRoundedRectangle(body.reduced(0.5f), r, 1.0f);

        auto textArea = bodyInt.withTrimmedRight(jmin(bodyInt.getHeight(), bodyInt.getWidth()))
                            .withTrimmedLeft(jmin(4, bodyInt.getWidth()));

        g.setFont(Font(13.0f));
        g.setColour(currentPath.isEmpty() ? textCol.withAlpha(0.4f) : textCol.withAlpha(0.85f));
        g.drawText(currentPath.isEmpty() ? getPlaceholderText() : File(currentPath).getFileName(),
                   textArea,
                   Justification::centredLeft,
                   true);
    }

    bool isInterestedInFileDrag(const StringArray&) override { return currentPath.isEmpty(); }

    void filesDropped(const StringArray& files, int, int) override
    {
        if (! files.isEmpty())
        {
            File f(files[0]);

            if (f.existsAsFile())
                setAndNotify(f.getFullPathName());
        }
    }

    void setPath(const String& path)
    {
        currentPath = path;
        actionButton.setMode(currentPath.isEmpty() ? chooseFileModeInfo.displayLabel
                                                   : removeFileModeInfo.displayLabel);
        repaint();
    }

    void setFileTypes(const std::vector<std::string>& types) { fileTypes = types; }

    void setRequired(bool req)
    {
        required = req;
        repaint();
    }

    int getPreferredWidth() const override { return minFilePickerWidth; }

    int getPreferredHeight() const override
    {
        return minFilePickerHeight + (required ? 0 : bannerHeight);
    }

    int getMinimumRequiredWidth() const override
    {
        const int labelWidth = getLabelWidth(label);
        return jmax(minFilePickerWidth, labelWidth + defaultPadding);
    }

    std::function<void(const String&)> onFileSelected;

private:
    void initializeButton()
    {
        chooseFileModeInfo = MultiButton::Mode { "ChooseFile",
                                                 "Click to choose a file.",
                                                 [this] { launchFileChooser(); },
                                                 MultiButton::DrawingMode::IconOnly,
                                                 Colours::lightblue,
                                                 fontawesome::Folder };

        removeFileModeInfo = MultiButton::Mode { "RemoveFile",
                                                 "Click to remove the selected file.",
                                                 [this] { clearFile(); },
                                                 MultiButton::DrawingMode::IconOnly,
                                                 Colours::orangered,
                                                 fontawesome::Remove };

        actionButton.addMode(chooseFileModeInfo);
        actionButton.addMode(removeFileModeInfo);
        actionButton.setMode(chooseFileModeInfo.displayLabel);

        actionButton.setLookAndFeel(&noBorderLAF);
    }

    String getPlaceholderText() const
    {
        if (fileTypes.empty())
            return "No file selected";

        StringArray exts;

        for (const auto& ext : fileTypes)
        {
            String e(ext);
            exts.add(e.startsWithChar('.') ? e : "." + e);
        }

        return "No file selected (" + exts.joinIntoString(", ") + ")";
    }

    void clearFile()
    {
        setPath({});

        if (onFileSelected)
            onFileSelected({});
    }

    void setAndNotify(const String& path)
    {
        setPath(path);

        if (onFileSelected)
            onFileSelected(path);
    }

    void launchFileChooser()
    {
        fileChooser = std::make_unique<FileChooser>("Select file", File(), buildWildcardPattern());

        fileChooser->launchAsync(FileBrowserComponent::openMode
                                     | FileBrowserComponent::canSelectFiles,
                                 [this](const FileChooser& chooser)
                                 {
                                     const File f = chooser.getResult();

                                     if (f.existsAsFile())
                                         setAndNotify(f.getFullPathName());
                                 });
    }

    String buildWildcardPattern() const
    {
        if (fileTypes.empty())
            return "*";

        StringArray patterns;

        for (const auto& ext : fileTypes)
        {
            String e(ext);

            if (! e.startsWithChar('.'))
                e = "." + e;

            patterns.add("*" + e);
        }

        return patterns.joinIntoString(";");
    }

    struct NoBorderLookAndFeel : public LookAndFeel_V4
    {
        void drawButtonBackground(Graphics&, Button&, const Colour&, bool, bool) override {}
    };

    static constexpr int minFilePickerWidth = 260;
    static constexpr int minFilePickerHeight = 50;
    static constexpr int labelHeight = 20;
    static constexpr int bannerHeight = 14;

    NoBorderLookAndFeel noBorderLAF;
    Label label;
    MultiButton actionButton;
    MultiButton::Mode chooseFileModeInfo;
    MultiButton::Mode removeFileModeInfo;

    String currentPath;
    bool required = true;
    std::vector<std::string> fileTypes;
    std::unique_ptr<FileChooser> fileChooser;
};
