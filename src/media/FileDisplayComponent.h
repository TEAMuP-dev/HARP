/**
 * @file FileDisplayComponent.h
 * @brief Media display for a generic file track (neither audio nor MIDI).
 * @author cwitkowitz, derekllanes
 */

#pragma once

#include "MediaDisplayComponent.h"

class FileDisplayComponent : public MediaDisplayComponent
{
public:
    FileDisplayComponent();
    FileDisplayComponent(String name,
                         bool req = true,
                         bool fromDAW = false,
                         DisplayMode mode = DisplayMode::Input);
    ~FileDisplayComponent() override;

    static StringArray getSupportedExtensions();

    StringArray getInstanceExtensions() override;

    int getFixedHeight() const override { return fixedHeight; }

    double getTotalLengthInSecs() override;

    bool hasPlaybackCursor() const override { return false; }
    bool usesMouseWheel() const override { return false; }

    void paint(Graphics& g) override;
    void resized() override;

    void loadMediaFile(const URL& filePath) override;
    void resetMedia() override;

    void postLoadActions(const URL& filePath) override;

    // Extensions with a leading dot (e.g., ".json"), where none means any file
    void setInstanceFileTypes(const StringArray& extensions) { instanceFileTypes = extensions; }

private:
    BorderlessButtonLookAndFeel noBorderLAF;

    MultiButton downloadButton;
    MultiButton::Mode downloadActiveMode;
    MultiButton::Mode downloadInactiveMode;

    StringArray instanceFileTypes;

    static constexpr int fixedHeight = 50;
    static constexpr int labelHeight = 20;
};
