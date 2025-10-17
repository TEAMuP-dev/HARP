#pragma once

#include "../WebModel.h"
#include "../client/GradioClient.h"
#include "../client/StabilityClient.h"
#include "../external/magic_enum.hpp"
#include <JuceHeader.h>

class LoginTab : public juce::Component
{
public:
    enum Provider
    {
        HUGGINGFACE,
        STABILITY,
        UNKNOWN
    };

    LoginTab(const juce::String& providerName, WebModel* m);
    ~LoginTab() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    juce::Label titleLabel;
    juce::Label infoLabel;
    juce::Label registerLabel;
    juce::HyperlinkButton linkLabel;

    WebModel* currentlyLoadedModel;

    juce::Label statusLabel;
    LoginTab::Provider provider;
    juce::TextEditor userToken;
    juce::TextButton submitButton { "Submit" };
    juce::TextButton forgetButton { "Remove Token" };

    LoginTab::Provider getProvider(const juce::String& providerName);
    juce::String getStorageKey();
    juce::URL getTokenURL();
    OpResult validateToken(const juce::String& token);

    void handleForget();
    void handleSubmit();

    void setStatus(juce::String text);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoginTab)
};
