#include "LoginTab.h"

ProviderPage::ProviderPage(Provider p) : provider(p)
{
    client = createClient(provider);
    client->setRequestRegistry(&requestRegistry);

    titleLabel.setText("Update or remove your API key for " + getDisplayName() + ".",
                       dontSendNotification);
    titleLabel.setJustificationType(Justification::centred);

    getTokenLink.setButtonText("Click here to register a new API key.");
    getTokenLink.setURL(client->tokenRegistrationURL);
    getTokenLink.setColour(HyperlinkButton::textColourId, Colours::blue);

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(getTokenLink);

    tokenEditor.setTextToShowWhenEmpty("Enter API key here", Colours::grey);
    tokenEditor.setReturnKeyStartsNewLine(false);
    tokenEditor.setSelectAllWhenFocused(true);

    // Keep the key obscured by default so it is not revealed by a screen share
    tokenEditor.setPasswordCharacter(passwordCharacter);
    tokenEditor.onTextChange = [this] { updateButtons(); };
    tokenEditor.onReturnKey = [this] { updateButton.triggerClick(); };

    addAndMakeVisible(tokenEditor);

    revealTokenButton.onClick = [this]
    {
        tokenEditor.setPasswordCharacter(revealTokenButton.getToggleState() ? 0
                                                                            : passwordCharacter);
    };

    addAndMakeVisible(revealTokenButton);

    updateButton.addShortcut(KeyPress(KeyPress::returnKey));
    updateButton.onClick = [this] { updateTokenCallback(); };
    removeButton.onClick = [this] { removeTokenCallback(); };

    addAndMakeVisible(updateButton);
    addAndMakeVisible(removeButton);

    statusLabel.setJustificationType(Justification::centred);

    addAndMakeVisible(statusLabel);

    updateButtons();
}

ProviderPage::~ProviderPage()
{
    /* A check in flight would leave its worker blocked in a network call, and destroying
       the pool under it would force-kill the thread. Abort the request so the worker
       returns at once, then wait for it. */
    requestRegistry.abortActiveRequests();
    checkPool.removeAllJobs(true, 5000);
}

void ProviderPage::resized()
{
    FlexBox pageArea;
    pageArea.flexDirection = FlexBox::Direction::column;

    pageArea.items.add(FlexItem(titleLabel).withHeight(rowHeight).withMargin(marginSize));
    pageArea.items.add(FlexItem().withHeight(gapSize)); // Filler space
    pageArea.items.add(FlexItem(tokenEditor).withHeight(rowHeight).withMargin(marginSize));
    pageArea.items.add(FlexItem(revealTokenButton).withHeight(rowHeight).withMargin(marginSize));

    FlexBox buttonsArea;
    buttonsArea.flexDirection = FlexBox::Direction::row;

    buttonsArea.items.add(FlexItem().withFlex(1.0)); // Filler space
    buttonsArea.items.add(FlexItem(updateButton).withWidth(buttonWidth));
    buttonsArea.items.add(FlexItem().withWidth(gapSize)); // Filler space
    buttonsArea.items.add(FlexItem(removeButton).withWidth(buttonWidth));
    buttonsArea.items.add(FlexItem().withFlex(1.0)); // Filler space

    pageArea.items.add(FlexItem().withHeight(gapSize)); // Filler space
    pageArea.items.add(FlexItem(buttonsArea).withHeight(rowHeight));
    pageArea.items.add(FlexItem().withFlex(1.0)); // Filler space

    pageArea.items.add(FlexItem(statusLabel).withHeight(rowHeight));
    pageArea.items.add(FlexItem().withFlex(1.0)); // Filler space

    pageArea.items.add(FlexItem(getTokenLink).withHeight(rowHeight).withMargin(marginSize));

    pageArea.performLayout(getLocalBounds());
}

void ProviderPage::resetState()
{
    const String savedKey = sharedTokens->getKey(provider);

    tokenEditor.setText(savedKey, dontSendNotification);
    updateButtons();

    if (savedKey.isEmpty())
    {
        // Supersedes any check still in flight
        ++latestCheckID;

        statusLabel.setText("No key has been added for this provider.", dontSendNotification);
    }
    else
    {
        checkKey(savedKey, false);
    }
}

void ProviderPage::updateButtons()
{
    const String enteredKey = tokenEditor.getText().trim();
    const String savedKey = sharedTokens->getKey(provider);

    // A new key can be saved, and the saved one removed while it is the one shown
    updateButton.setEnabled(enteredKey.isNotEmpty() && enteredKey != savedKey);
    removeButton.setEnabled(savedKey.isNotEmpty() && enteredKey == savedKey);
}

void ProviderPage::updateTokenCallback()
{
    const String enteredKey = tokenEditor.getText().trim();

    if (enteredKey.isNotEmpty())
    {
        updateButton.setEnabled(false);

        checkKey(enteredKey, true);
    }
}

void ProviderPage::removeTokenCallback()
{
    sharedTokens->removeKey(provider);

    tokenEditor.clear();

    // Supersedes any check still in flight
    ++latestCheckID;

    statusLabel.setText("No key has been added for this provider.", dontSendNotification);

    updateButtons();
}

void ProviderPage::checkKey(const String& key, bool saveIfValid)
{
    const int checkID = ++latestCheckID;

    statusLabel.setText("Checking the key...", dontSendNotification);

    SafePointer<ProviderPage> safeThis(this);

    checkPool.addJob(
        [this, safeThis, key, saveIfValid, checkID]
        {
            OpResult result = client->validateToken(key);

            MessageManager::callAsync(
                [safeThis, result, key, saveIfValid, checkID]
                {
                    if (safeThis != nullptr)
                        safeThis->showCheckResult(result, key, saveIfValid, checkID);
                });
        });
}

void ProviderPage::showCheckResult(const OpResult& result,
                                   const String& key,
                                   bool saveIfValid,
                                   int checkID)
{
    if (checkID != latestCheckID)
    {
        return;
    }

    if (result.failed())
    {
        statusLabel.setText(describeCheckFailure(result.getError()), dontSendNotification);
    }
    else if (saveIfValid)
    {
        sharedTokens->updateKey(provider, key);

        statusLabel.setText("This key is valid, and has been saved.", dontSendNotification);
    }
    else
    {
        statusLabel.setText("This key is valid.", dontSendNotification);
    }

    updateButtons();
}

String ProviderPage::describeCheckFailure(const Error& error) const
{
    // An unreachable provider says nothing about the key itself
    if (const auto* e = std::get_if<HttpError>(&error);
        e != nullptr && e->type == HttpError::Type::ConnectionFailed)
    {
        return "Could not reach " + getDisplayName() + " to check this key.";
    }

    if (const auto* e = std::get_if<ClientError>(&error);
        e != nullptr && e->type == ClientError::Type::InsufficientPermissions)
    {
        return "This key does not have sufficient permissions.";
    }

    return "This key is invalid.";
}

void PageSwitcher::resized()
{
    for (auto& e : entries)
    {
        e.page->setBounds(getLocalBounds());
    }
}

int PageSwitcher::getIndexForProvider(Provider provider) const
{
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        if (entries[(size_t) i].page->getProvider() == provider)
        {
            return i;
        }
    }

    return -1;
}

void PageSwitcher::showPage(int idx)
{
    for (int i = 0; i < getNumPages(); ++i)
    {
        auto& page = *entries[(size_t) i].page;

        page.setVisible(i == idx);

        if (i == idx)
        {
            page.resetState();
        }
    }
}

void PageSwitcher::addPage(std::unique_ptr<ProviderPage> page)
{
    // Shown, and so checked, once its row is selected
    addChildComponent(page.get());

    const String displayName = page->getDisplayName();

    entries.push_back({ displayName, std::move(page) });
}

LoginTab::LoginTab()
{
    sidebar.setModel(this);
    sidebar.setRowHeight(36);
    sidebar.setOutlineThickness(0);
    addAndMakeVisible(sidebar);

    loginPages.addPage(std::make_unique<ProviderPage>(Provider::HuggingFace));
    loginPages.addPage(std::make_unique<ProviderPage>(Provider::Stability));
    addAndMakeVisible(loginPages);

    sidebar.updateContent();
    sidebar.selectRow(0);
}

void LoginTab::showProvider(Provider provider)
{
    const int idx = loginPages.getIndexForProvider(provider);

    if (idx < 0)
    {
        DBG_AND_LOG("LoginTab::showProvider: No page serves the requested provider.");

        return;
    }

    /* Selecting the row rather than the page directly, so the sidebar highlight and
       the visible page cannot disagree. */
    sidebar.selectRow(idx);
}

void LoginTab::selectedRowsChanged(int row)
{
    // Clicking below the last row deselects every row, which leaves the page showing
    if (row >= 0)
    {
        loginPages.showPage(row);
    }
}

void LoginTab::resized()
{
    Rectangle<int> area = getLocalBounds();

    sidebar.setBounds(area.removeFromLeft(100));

    loginPages.setBounds(area);
}

void LoginTab::paintListBoxItem(int rowNumber,
                                Graphics& g,
                                int width,
                                int height,
                                bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.fillAll(Colours::darkgrey.withAlpha(0.4f));
    }

    g.setColour(Colours::white);
    g.setFont(14.0f);
    g.drawText(loginPages.getNameForIndex(rowNumber), 0, 0, width, height, Justification::centred);
}
