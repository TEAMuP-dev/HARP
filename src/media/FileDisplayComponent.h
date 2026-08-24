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

    /* A generic file has no timeline, so the playback cursor would only ever be a
       stray sliver drawn at the top of the track. */
    bool hasPlaybackCursor() const override { return false; }

    void paint(Graphics& g) override;
    void resized() override;

    void loadMediaFile(const URL& filePath) override;
    void resetMedia() override;

    void postLoadActions(const URL& filePath) override;

    void setInstanceFileTypes(const std::vector<std::string>& types);

private:
    struct NoBorderLookAndFeel : public LookAndFeel_V4
    {
        void drawButtonBackground(Graphics&, Button&, const Colour&, bool, bool) override {}
    };

    NoBorderLookAndFeel noBorderLAF;

    MultiButton downloadButton;
    MultiButton::Mode downloadActiveMode;
    MultiButton::Mode downloadInactiveMode;

    StringArray instanceFileTypes;

    static constexpr int fixedHeight = 50;
    static constexpr int labelHeight = 20;
};
