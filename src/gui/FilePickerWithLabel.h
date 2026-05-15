#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ControlComponent.h"
#include "../utils/Controls.h"

using namespace juce;

class FilePickerWithLabel : public ControlComponent, private Button::Listener
{
public:
    explicit FilePickerWithLabel(FilePickerComponentInfo* infoToUse)
        : info(infoToUse),
          browseButton("Browse...")
    {
        if (info != nullptr)
        {
            label.setText(info->label, dontSendNotification);
        }

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

    ~FilePickerWithLabel() override
    {
        browseButton.removeListener(this);
    }

    void resized() override
    {
        auto area = getLocalBounds();

        auto labelArea = area.removeFromTop(20);
        label.setBounds(labelArea);

        area.removeFromTop(4);

        auto buttonArea = area.removeFromLeft(90);
        browseButton.setBounds(buttonArea);

        area.removeFromLeft(6);
        pathBox.setBounds(area);
    }

    int getMinimumRequiredWidth() const override
    {
        const int labelWidth = getLabelWidth(label);
        return jmax(260, labelWidth + defaultPadding);
    }

private:
    void buttonClicked(Button* button) override
    {
        if (button != &browseButton || info == nullptr)
        {
            return;
        }

        String pattern = buildWildcardPattern();

        fileChooser = std::make_unique<FileChooser>(
            "Select file",
            File(),
            pattern
        );

        fileChooser->launchAsync(
            FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
            [this](const FileChooser& chooser)
            {
                File selectedFile = chooser.getResult();

                if (selectedFile.existsAsFile())
                {
                    info->path = selectedFile.getFullPathName().toStdString();
                    pathBox.setText(selectedFile.getFullPathName(), dontSendNotification);
                }
            }
        );
    }

    String buildWildcardPattern() const
    {
        if (info == nullptr || info->fileTypes.empty())
        {
            return "*";
        }

        StringArray patterns;

        for (const auto& ext : info->fileTypes)
        {
            String extension(ext);

            if (! extension.startsWithChar('.'))
            {
                extension = "." + extension;
            }

            patterns.add("*" + extension);
        }

        return patterns.joinIntoString(";");
    }

    FilePickerComponentInfo* info = nullptr;

    Label label;
    TextEditor pathBox;
    TextButton browseButton;

    std::unique_ptr<FileChooser> fileChooser;
};