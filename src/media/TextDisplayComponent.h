#pragma once

#include "MediaDisplayComponent.h"

class TextDisplayComponent : public MediaDisplayComponent
{
public:
    TextDisplayComponent();
    TextDisplayComponent(String name,
                         bool req = true,
                         bool fromDAW = false,
                         DisplayMode mode = DisplayMode::Input);
    ~TextDisplayComponent() override = default;

    static StringArray getSupportedExtensions();

    StringArray getInstanceExtensions() override;

    int getFixedHeight() const override { return fixedHeight; }

    double getTotalLengthInSecs() override;

    void paint(Graphics& g) override;
    void resized() override;

    void loadMediaFile(const URL& filePath) override;
    void resetMedia() override;

    void postLoadActions(const URL& filePath) override;

private:
    TextEditor textEditor;

    static constexpr int fixedHeight = 150;
};
