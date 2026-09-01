/**
 * @file TextDisplayComponent.h
 * @brief Display specifics for text (txt).
 * @author ChughVansh
 */

#pragma once

#include "MediaDisplayComponent.h"

class TextDisplayComponent : public MediaDisplayComponent
{
public:
    TextDisplayComponent();
    TextDisplayComponent(String name,
                         bool req = true,
                         bool fromDAW = false,
                         DisplayMode mode = DisplayMode::Hybrid);
    ~TextDisplayComponent() override = default;

    static StringArray getSupportedExtensions();

    StringArray getInstanceExtensions() override
    {
        return TextDisplayComponent::getSupportedExtensions();
    }

    int getFixedHeight() const override { return fixedHeight; }

    bool supportsPlayback() const override { return false; }

    bool usesMouseWheel() const override { return false; }

    double getTotalLengthInSecs() override { return 0.0; }

    void resized() override;

    void loadMediaFile(const URL& filePath) override;

private:
    void resetMedia() override;

    void postLoadActions(const URL& filePath) override;

    TextEditor textEditor;

    static constexpr int fixedHeight = 150;
};
