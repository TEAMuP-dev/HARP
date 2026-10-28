/**
 * @file LoginTab.h
 * @brief Tab with login for each API provider.
 * @author cwitkowitz, huiranyu
 */

#pragma once

#include <JuceHeader.h>

#include "../../clients/Client.h"

#include "../../utils/Clients.h"
#include "../../utils/Logging.h"
#include "../../utils/Settings.h"

using namespace juce;

class ProviderPage : public Component
{
public:
    explicit ProviderPage(Provider p);
    ~ProviderPage() override;

    void resized() override;

    String getDisplayName() const { return getProviderName(provider); }

    Provider getProvider() const { return provider; }

    // Shows the saved key, and checks it with the provider
    void resetState();

private:
    void updateTokenCallback();
    void removeTokenCallback();

    // Enables the buttons that apply to the key in the editor
    void updateButtons();

    /* Checks a key with the provider in the background, since that is a network request,
       and saves it once it proves valid if saveIfValid is set */
    void checkKey(const String& key, bool saveIfValid);
    void showCheckResult(const OpResult& result, const String& key, bool saveIfValid, int checkID);

    String describeCheckFailure(const Error& error) const;

    const float marginSize = 2;
    const float gapSize = 10;
    const float rowHeight = 26;
    const float buttonWidth = 120;

    // Character used to obscure the API key in the editor
    static constexpr juce_wchar passwordCharacter = 0x2022; // Bullet

    Provider provider;

    Label titleLabel;
    HyperlinkButton getTokenLink;

    TextEditor tokenEditor;
    ToggleButton revealTokenButton { "Show key" };

    TextButton updateButton { "Update" };
    TextButton removeButton { "Remove" };

    Label statusLabel;

    // Identifies the latest check, so that the result of an earlier one is not shown
    int latestCheckID = 0;

    SharedResourcePointer<SharedAPIKeys> sharedTokens;

    // Declared before the client, which holds on to it
    RequestRegistry requestRegistry;
    std::unique_ptr<Client> client;

    // Declared last so that it is destroyed first, while what its job uses still exists
    ThreadPool checkPool { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProviderPage)
};

class PageSwitcher : public Component
{
public:
    struct Entry
    {
        String displayName;
        std::unique_ptr<ProviderPage> page;
    };

    void resized() override;

    int getNumPages() const { return (int) entries.size(); }

    const String& getNameForIndex(int idx) const { return entries[(size_t) idx].displayName; }

    /** Index of the page for a provider, or -1 when none of them serves it. */
    int getIndexForProvider(Provider provider) const;

    void showPage(int idx);
    void addPage(std::unique_ptr<ProviderPage> page);

private:
    std::vector<Entry> entries;
};

class LoginTab : public Component, private ListBoxModel
{
public:
    LoginTab();

    void resized() override;

    /** Brings the page for a provider to the front. */
    void showProvider(Provider provider);

private:
    int getNumRows() override { return loginPages.getNumPages(); }

    void paintListBoxItem(int rowNumber,
                          Graphics& g,
                          int width,
                          int height,
                          bool rowIsSelected) override;

    void selectedRowsChanged(int row) override;

    ListBox sidebar;
    PageSwitcher loginPages;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoginTab)
};
