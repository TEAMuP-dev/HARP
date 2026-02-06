/**
 * @file LoginTab.h
 * @brief Tab with login for each API provider.
 * @author huiranyu, cwitkowitz
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
    ProviderPage(Provider p, String n);

    void resized() override;

    String getDisplayName() { return displayName; }

    void resetState();

private:
    void updateTokenCallback();
    void removeTokenCallback();

    const float marginSize = 2;
    const float gapSize = 10;
    const float rowHeight = 26;
    const float buttonWidth = 120;

    Provider provider;
    String displayName;

    Label titleLabel;
    HyperlinkButton getTokenLink;

    TextEditor tokenEditor;

    TextButton updateButton { "Update" };
    TextButton removeButton { "Remove" };

    std::unique_ptr<Client> client;

    String tokenMissingMessage = "No token has been added for this provider.";
    String tokenInvalidMessage = "This token is invalid.";
    String tokenLoadedMessage = "This token has been loaded.";
    Label statusLabel;

    SharedResourcePointer<SharedAPIKeys> sharedTokens;

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

    int getNumPages() { return entries.size(); }

    const String& getNameForIndex(int idx) const { return entries[(size_t) idx].displayName; }

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

private:
    int getNumRows() override { return loginPages.getNumPages(); }

    void paintListBoxItem(int rowNumber,
                          Graphics& g,
                          int width,
                          int height,
                          bool rowIsSelected) override;

    void selectedRowsChanged(int row) override { loginPages.showPage(row); }

    ListBox sidebar;
    PageSwitcher loginPages;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoginTab)
};
