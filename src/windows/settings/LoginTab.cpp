#include "LoginTab.h"

ProviderPage::ProviderPage(Provider p, String n) : provider(p), displayName(n)
{
    // Load appropriate client
    client = multiplexClients(provider);

    String titleText = "Update or remove your API key for " + displayName + ".";

    titleLabel.setText(titleText, dontSendNotification);
    titleLabel.setJustificationType(Justification::centred);

    getTokenLink.setButtonText("Click here to register a new API key.");
    getTokenLink.setURL(client->tokenRegistrationURL);
    getTokenLink.setColour(HyperlinkButton::textColourId, Colours::blue);

    addAndMakeVisible(titleLabel);
    addAndMakeVisible(getTokenLink);

    String emptyText = "Enter API key here";

    tokenEditor.setTextToShowWhenEmpty(emptyText, Colours::grey);
    tokenEditor.setReturnKeyStartsNewLine(false);
    tokenEditor.setSelectAllWhenFocused(true);
    tokenEditor.onTextChange = [this]()
    {
        String currentText = tokenEditor.getText().trim();

        if (currentText.isNotEmpty())
        {
            if (sharedTokens->savedTokens.contains(provider))
            {
                String savedToken = sharedTokens->savedTokens[provider];

                if (currentText == savedToken)
                {
                    updateButton.setEnabled(false);
                    removeButton.setEnabled(true);
                }
                else
                {
                    updateButton.setEnabled(true);
                    removeButton.setEnabled(false);
                }
            }
            else
            {
                updateButton.setEnabled(true);
                removeButton.setEnabled(false);
            }
        }
        else
        {
            updateButton.setEnabled(false);
            removeButton.setEnabled(false);
        }
    };
    tokenEditor.onReturnKey = [this] { updateButton.triggerClick(); };

    addAndMakeVisible(tokenEditor);

    updateButton.addShortcut(KeyPress(KeyPress::returnKey));
    updateButton.onClick = [this] { updateTokenCallback(); };
    removeButton.onClick = [this] { removeTokenCallback(); };

    resetState();

    addAndMakeVisible(updateButton);
    addAndMakeVisible(removeButton);

    statusLabel.setJustificationType(Justification::centred);

    addAndMakeVisible(statusLabel);
}

void ProviderPage::resized()
{
    FlexBox pageArea;
    pageArea.flexDirection = FlexBox::Direction::column;

    pageArea.items.add(FlexItem(titleLabel).withHeight(rowHeight).withMargin(marginSize));
    pageArea.items.add(FlexItem().withHeight(gapSize)); // Filler space
    pageArea.items.add(FlexItem(tokenEditor).withHeight(rowHeight).withMargin(marginSize));

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
    if (sharedTokens->savedTokens.contains(provider))
    {
        String savedToken = sharedTokens->savedTokens[provider];

        OpResult result = client->validateToken(savedToken);

        if (result.failed())
        {
            // TODO - could be due to connection failure or invalid response as well
            statusLabel.setText(std::move(tokenInvalidMessage), dontSendNotification);
        }
        else
        {
            statusLabel.setText(std::move(tokenLoadedMessage), dontSendNotification);
        }

        tokenEditor.setText(savedToken);
        removeButton.setEnabled(true);
    }
    else
    {
        statusLabel.setText(std::move(tokenMissingMessage), dontSendNotification);
        updateButton.setEnabled(false);
        removeButton.setEnabled(false);
    }
}

void ProviderPage::updateTokenCallback()
{
    String enteredToken = tokenEditor.getText().trim();

    if (enteredToken.isNotEmpty())
    {
        OpResult result = client->validateToken(enteredToken);

        if (result.failed())
        {
            // TODO - could be due to connection failure or invalid response as well
            statusLabel.setText(std::move(tokenInvalidMessage), dontSendNotification);
        }
        else
        {
            sharedTokens->updateKey(provider, enteredToken);

            statusLabel.setText(std::move(tokenLoadedMessage), dontSendNotification);
            removeButton.setEnabled(true);
        }

        updateButton.setEnabled(false);
    }
    else
    {
        statusLabel.setText(std::move(tokenMissingMessage), dontSendNotification);
    }
}

void ProviderPage::removeTokenCallback()
{
    if (sharedTokens->savedTokens.contains(provider))
    {
        sharedTokens->removeKey(provider);

        tokenEditor.clear();

        statusLabel.setText(std::move(tokenMissingMessage), dontSendNotification);

        updateButton.setEnabled(false);
        removeButton.setEnabled(false);
    }
}

void PageSwitcher::resized()
{
    for (auto& e : entries)
    {
        e.page->setBounds(getLocalBounds());
    }
}

void PageSwitcher::showPage(int idx)
{
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        if (i == idx)
        {
            entries[i].page->setVisible(true);
            entries[i].page->resetState();
        }
        else
        {
            entries[i].page->setVisible(false);
        }
    }
}

void PageSwitcher::addPage(std::unique_ptr<ProviderPage> page)
{
    addAndMakeVisible(page.get());
    page->setVisible(entries.empty());

    entries.push_back({ std::move(page->getDisplayName()), std::move(page) });
}

LoginTab::LoginTab()
{
    sidebar.setModel(this);
    sidebar.setRowHeight(36);
    sidebar.setOutlineThickness(0);
    addAndMakeVisible(sidebar);

    loginPages.addPage(std::make_unique<ProviderPage>(Provider::HuggingFace, "Hugging Face"));
    loginPages.addPage(std::make_unique<ProviderPage>(Provider::Stability, "Stability AI"));
    addAndMakeVisible(loginPages);

    sidebar.updateContent();
    sidebar.selectRow(0);
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
