#include "LoginTab.h"
#include "../AppSettings.h"
#include "../HarpLogger.h"

LoginTab::LoginTab(const juce::String& providerName)
{
    // Setup toggle button
    provider = getProvider(providerName);
    if (provider == LoginTab::Provider::UNKNOWN)
    {
        DBG("Invalid provider name passed to loginToProvider()");
        return;
    }

    bool isHuggingFace = (provider == LoginTab::Provider::HUGGINGFACE);
    // Set provider-specific values
    juce::String title =
        "Login to " + juce::String(isHuggingFace ? "Hugging Face" : "Stability AI");
    juce::String message =
        "Paste your "
        + juce::String(isHuggingFace ? "Hugging Face access token" : "Stability AI API token")
        + " below.";
    
    juce::String tokenLabel = isHuggingFace ? "Access Token" : "API Token";

    // Create token prompt window
    titleLabel.setText(title, juce::dontSendNotification);
    infoLabel.setText(message, juce::dontSendNotification);
    registerLabel.setText("New User?", juce::dontSendNotification);
    linkLabel.setButtonText("Get token");
    linkLabel.setURL(getTokenURL());
    linkLabel.setFont(juce::Font(registerLabel.getFont().getHeight(), juce::Font::FontStyleFlags::underlined), false, juce::Justification::centredLeft);

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(infoLabel);
    addAndMakeVisible(registerLabel);
    addAndMakeVisible(linkLabel);

    addAndMakeVisible(statusLabel);

    userToken.setTextToShowWhenEmpty(tokenLabel, juce::Colours::grey);
    userToken.setReturnKeyStartsNewLine(false);
    userToken.setSelectAllWhenFocused(true);
    userToken.onTextChange = [this] () {
        bool hasText = userToken.getText().trim().isNotEmpty();
        submitButton.setEnabled(hasText);
    };
    userToken.onReturnKey = [this] { submitButton.triggerClick(); };
    addAndMakeVisible(userToken);

    // rememberTokenToggle.setButtonText("Remember this token");
    // rememberTokenToggle.setSize(200, 24);
    // addAndMakeVisible(rememberTokenToggle);

    juce::String savedToken = AppSettings::getString(getStorageKey());
    if (savedToken.isNotEmpty()) {
        OpResult result = validateToken(savedToken);
        if (result.failed()) {
            setStatus("Saved token invalid. Please apply for another token.");
            forgetButton.setEnabled(true);
        }
        else {
            userToken.setText(savedToken);
            setStatus("Token verified.");
            forgetButton.setEnabled(true);
        }
    }
    else {
        submitButton.setEnabled(false);
        forgetButton.setEnabled(false);
    }

    submitButton.addShortcut(juce::KeyPress(juce::KeyPress::returnKey));
    submitButton.onClick = [this] { handleSubmit(); };
    // tokenButton.onClick = [this] { handleToken(); };
    forgetButton.onClick = [this] { handleForget(); };

    addAndMakeVisible(forgetButton);
    addAndMakeVisible(submitButton);
    // addAndMakeVisible(tokenButton);
}

LoginTab::Provider LoginTab::getProvider(const juce::String& providerName) {
    static const juce::StringArray names {"huggingface", "stability"};
    auto i = names.indexOf(providerName.trim().toLowerCase());
    return (i >= 0) ? static_cast<LoginTab::Provider>(i) : LoginTab::Provider::UNKNOWN;
}

juce::String LoginTab::getStorageKey() {
    switch (provider) {
        case LoginTab::Provider::HUGGINGFACE:
            return "huggingFaceToken";
            break;
        case LoginTab::Provider::STABILITY:
            return "stabilityToken";
            break;
        default:
            return "";
    }
}

OpResult LoginTab::validateToken(const juce::String& token) {
    switch (provider) {
        case LoginTab::Provider::HUGGINGFACE:
            return GradioClient().validateToken(token);
            break;
        case LoginTab::Provider::STABILITY:
            return StabilityClient().validateToken(token);
            break;
        default:
            Error error;
            error.type = ErrorType::InvalidURL;
            error.devMessage = "Unknown Provider";
            return OpResult::fail(error);
    }
}

void LoginTab::handleSubmit() {
    auto token = userToken.getText().trim();
    if (token.isNotEmpty())
    {
        // Validate token
        OpResult result = validateToken(token);

        if (result.failed())
        {
            Error err = result.getError();
            Error::fillUserMessage(err);
            LogAndDBG("Invalid token:\n" + err.devMessage.toStdString());
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                                "Invalid Token",
                                                "The provided token is invalid:\n"
                                                    + err.userMessage);
            juce::String savedToken = AppSettings::getString(getStorageKey());
            if (savedToken.isNotEmpty()) {
                userToken.setText(savedToken);
                setStatus("Invalid token. Saved token restored.");
            }
            else {
                userToken.clear();
                submitButton.setEnabled(false);
                setStatus("Invalid token.");
            }
        }
        else
        {                
            AppSettings::setValue(getStorageKey(), token);
            AppSettings::saveIfNeeded();
            setStatus("Token verified and saved.");
            forgetButton.setEnabled(true);
        }
    }
    else
    {
        setStatus("No token entered.");
    }
}

juce::URL LoginTab::getTokenURL() {
    juce::String tokenURL;
    switch (provider)
    {
    case LoginTab::Provider::HUGGINGFACE:
        tokenURL = "https://huggingface.co/settings/tokens";
        break;
    case LoginTab::Provider::STABILITY:
        tokenURL = "https://platform.stability.ai/account/keys";
        break;
    default:
        break;
    }
    // juce::URL(tokenURL).launchInDefaultBrowser();
    return juce::URL(tokenURL);
}

void LoginTab::resized()
{
    DBG("LoginTab::resized()");
    auto area = getLocalBounds().reduced(10);
    const int rowH = 26;
    const int gap = 10;
    const int btnW = 120;
    // Messages
    titleLabel.setBounds(area.removeFromTop(rowH));
    area.removeFromTop(gap);
    infoLabel.setBounds(area.removeFromTop(rowH));
    area.removeFromTop(gap);
    auto registerRow = area.removeFromTop(rowH);
    registerLabel.setBounds(registerRow.removeFromLeft(100));
    linkLabel.setBounds(registerRow.removeFromLeft(btnW));

    // Token and Toggle
    userToken.setBounds(area.removeFromTop(rowH));
    // rememberTokenToggle.setBounds(area.removeFromTop(rowH));
    area.removeFromTop(gap);
    // Buttons
    auto buttonsRow = area.removeFromTop(rowH);
    const int totalBtnW = btnW * 2 + gap;
    auto right = buttonsRow.removeFromRight(totalBtnW);
    submitButton.setBounds(right.removeFromLeft(btnW));
    right.removeFromLeft(gap);
    forgetButton.setBounds(right.removeFromLeft(btnW));
    // right.removeFromLeft(gap);
    // tokenButton.setBounds(right); // remaining btnW
    // Status
    statusLabel.setBounds(area.removeFromBottom(rowH));
}

void LoginTab::paint(juce::Graphics& g)
{
    DBG("LoginTab::paint()");
    // g.fillAll(juce::Colours::lightgrey);  
}

void LoginTab::setStatus(juce::String text) {
    statusLabel.setText(std::move(text), juce::dontSendNotification);
}

void LoginTab::handleForget()
{   
    AppSettings::removeValue(getStorageKey());
    forgetButton.setEnabled(false);
    userToken.clear();
    submitButton.setEnabled(false);
    setStatus("Token removed");
}
