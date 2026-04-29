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

    double getTotalLengthInSecs() override;

    void loadMediaFile(const URL& filePath) override;
    void resetMedia() override;

    void postLoadActions(const URL& filePath) override;
};