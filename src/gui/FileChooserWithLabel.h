/**
 * @file FileChooserWithLabel.h
 * @brief Custom file chooser component with label.
 * @author derekllanes, cwitkowitz
 */

#pragma once

#include <string>
#include <vector>

#include "ControlComponent.h"

using namespace juce;

class FileChooserWithLabel : public ControlComponent, private Button::Listener
{
public:
    FileChooserWithLabel(const String& labelText = {})
    {
        label.setText(labelText, dontSendNotification);
        label.setJustificationType(Justification::centred);

        pathBox.setReadOnly(true);
        pathBox.setText("No file selected", dontSendNotification);
        pathBox.setMultiLine(false);
        pathBox.setScrollbarsShown(false);
        pathBox.setCaretVisible(false);

        browseButton.addListener(this);

        addAndMakeVisible(label);
        addAndMakeVisible(pathBox);
        addAndMakeVisible(browseButton);
    }

    ~FileChooserWithLabel() override
    {
        browseButton.removeListener(this);
    }

    void resized() override
    {
        auto area = getLocalBounds();

        label.setBounds(area.removeFromTop(labelHeight));

        area.removeFromTop(labelGap);

        browseButton.setBounds(area.removeFromLeft(browseButtonWidth));

        area.removeFromLeft(browseButtonGap);
        pathBox.setBounds(area);
    }

    void setPath(const String& path)
    {
        pathBox.setText(path, dontSendNotification);
    }

    void setFileTypes(const std::vector<std::string>& types)
    {
        fileTypes = types;
    }

    int getMinimumRequiredWidth() const override
    {
        const int labelWidth = getLabelWidth(label);
        return jmax(minFilePickerWidth, labelWidth + defaultPadding);
    }

    TextEditor& getPathBox() { return pathBox; }

    /** Called with full path after successful file choice. */
    std::function<void(const String&)> onFileSelected;

private:
    void buttonClicked(Button* button) override
    {
        if (button != &browseButton)
            return;

        fileChooser = std::make_unique<FileChooser>(
            "Select file",
            File(),
            buildWildcardPattern()
        );

        fileChooser->launchAsync(
            FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
            [this](const FileChooser& chooser)
            {
                const File selectedFile = chooser.getResult();

                if (selectedFile.existsAsFile())
                {
                    pathBox.setText(selectedFile.getFullPathName(), dontSendNotification);

                    if (onFileSelected)
                        onFileSelected(selectedFile.getFullPathName());
                }
            }
        );
    }

    String buildWildcardPattern() const
    {
        if (fileTypes.empty())
            return "*";

        StringArray patterns;

        for (const auto& ext : fileTypes)
        {
            String extension(ext);

            if (! extension.startsWithChar('.'))
                extension = "." + extension;

            patterns.add("*" + extension);
        }

        return patterns.joinIntoString(";");
    }

    static constexpr int minFilePickerWidth = 260;
    static constexpr int labelHeight = 20;
    static constexpr int labelGap = 4;
    static constexpr int browseButtonWidth = 90;
    static constexpr int browseButtonGap = 6;

    std::vector<std::string> fileTypes;

    Label label;
    TextEditor pathBox;
    TextButton browseButton { "Browse..." };

    std::unique_ptr<FileChooser> fileChooser;
};
