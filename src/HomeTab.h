/**
 * @file HomeTab.h
 * @brief Home tab for model discovery and loading.
 */

#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "utils/Interface.h"
#include "utils/ModelRegistry.h"
#include "widgets/ModelSelectionWidget.h"

using namespace juce;

class ModelRegistryCard : public Component
{
public:
    ModelRegistryCard(ModelRegistry::Entry registryEntry,
                      std::function<void(ModelRegistry::Entry)> loadCallback)
        : entry(std::move(registryEntry)), onLoad(std::move(loadCallback))
    {
        nameLabel.setText(entry.displayName, dontSendNotification);
        nameLabel.setJustificationType(Justification::centredLeft);
        nameLabel.setFont(Font(17.0f, Font::bold));
        addAndMakeVisible(nameLabel);

        providerLabel.setText(entry.provider, dontSendNotification);
        providerLabel.setJustificationType(Justification::centredLeft);
        providerLabel.setColour(Label::textColourId, Colours::lightgrey);
        addAndMakeVisible(providerLabel);

        summaryLabel.setText(entry.summary, dontSendNotification);
        summaryLabel.setJustificationType(Justification::centredLeft);
        summaryLabel.setColour(Label::textColourId, Colours::whitesmoke);
        addAndMakeVisible(summaryLabel);

        pathLabel.setText(entry.path, dontSendNotification);
        pathLabel.setJustificationType(Justification::centredLeft);
        pathLabel.setColour(Label::textColourId, Colours::grey);
        addAndMakeVisible(pathLabel);

        loadButton.setButtonText("Load");
        loadButton.onClick = [this]
        {
            if (onLoad)
                onLoad(entry);
        };
        addAndMakeVisible(loadButton);
    }

    void paint(Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::widgetBackground)
                        .brighter(0.06f));
        g.fillRoundedRectangle(bounds, 6.0f);

        g.setColour(Colours::white.withAlpha(0.12f));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12, 10);
        auto buttonArea = area.removeFromRight(92);
        loadButton.setBounds(buttonArea.withSizeKeepingCentre(80, 30));

        providerLabel.setBounds(area.removeFromTop(18));
        nameLabel.setBounds(area.removeFromTop(24));
        summaryLabel.setBounds(area.removeFromTop(24));
        pathLabel.setBounds(area.removeFromTop(18));
    }

    static constexpr int preferredHeight = 104;

private:
    ModelRegistry::Entry entry;
    std::function<void(ModelRegistry::Entry)> onLoad;

    Label nameLabel;
    Label providerLabel;
    Label summaryLabel;
    Label pathLabel;
    TextButton loadButton;
};

class ModelRegistryList : public Component
{
public:
    void setEntries(std::vector<ModelRegistry::Entry> newEntries,
                    std::function<void(ModelRegistry::Entry)> loadCallback)
    {
        cards.clear();
        removeAllChildren();

        for (auto& entry : newEntries)
        {
            auto card = std::make_unique<ModelRegistryCard>(std::move(entry), loadCallback);
            addAndMakeVisible(*card);
            cards.push_back(std::move(card));
        }

        resized();
        repaint();
    }

    void resized() override
    {
        auto area = getLocalBounds();

        for (auto& card : cards)
            card->setBounds(area.removeFromTop(ModelRegistryCard::preferredHeight).reduced(0, 4));
    }

    int getRequiredHeight() const
    {
        return static_cast<int>(cards.size()) * ModelRegistryCard::preferredHeight;
    }

private:
    std::vector<std::unique_ptr<ModelRegistryCard>> cards;
};

class HomeTab : public Component,
                private ChangeListener
{
public:
    HomeTab()
    {
        sharedChoices->addChangeListener(this);

        titleLabel.setText("Models", dontSendNotification);
        titleLabel.setJustificationType(Justification::centredLeft);
        titleLabel.setFont(Font(24.0f, Font::bold));

        subtitleLabel.setText("Search HARP-compatible models and open one in a new tab.",
                              dontSendNotification);
        subtitleLabel.setJustificationType(Justification::centredLeft);

        searchEditor.setTextToShowWhenEmpty("Search models...", Colours::grey);
        searchEditor.setMultiLine(false);
        searchEditor.setReturnKeyStartsNewLine(false);
        searchEditor.onTextChange = [this] { rebuildModelList(); };

        customPathButton.setButtonText("Custom Path");
        customPathButton.onClick = [this] { openCustomPathPopup(); };

        viewport.setViewedComponent(&modelList, false);
        viewport.setScrollBarsShown(true, false);

        addAndMakeVisible(titleLabel);
        addAndMakeVisible(subtitleLabel);
        addAndMakeVisible(searchEditor);
        addAndMakeVisible(customPathButton);
        addAndMakeVisible(viewport);

        rebuildModelList();
    }

    ~HomeTab() override
    {
        sharedChoices->removeChangeListener(this);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(16);

        titleLabel.setBounds(area.removeFromTop(34));
        subtitleLabel.setBounds(area.removeFromTop(26));

        area.removeFromTop(8);
        auto searchRow = area.removeFromTop(34);
        customPathButton.setBounds(searchRow.removeFromRight(120).reduced(0, 1));
        searchRow.removeFromRight(8);
        searchEditor.setBounds(searchRow);

        area.removeFromTop(10);
        viewport.setBounds(area);

        updateListBounds();
    }

    void resetSelection()
    {
        searchEditor.setEnabled(true);
        customPathButton.setEnabled(true);
        viewport.setEnabled(true);
    }

    Rectangle<int> getModelSelectBounds() const
    {
        return searchEditor.getBounds().expanded(2, 2);
    }

    std::function<void(String, String)> onModelLoadRequested;

private:
    void changeListenerCallback(ChangeBroadcaster* source) override
    {
        if (source == static_cast<SharedChoices*>(sharedChoices))
            rebuildModelList();
    }

    void requestModelLoad(const ModelRegistry::Entry& entry)
    {
        searchEditor.setEnabled(false);
        customPathButton.setEnabled(false);
        viewport.setEnabled(false);

        if (onModelLoadRequested)
            onModelLoadRequested(entry.path, entry.displayName);
    }

    void rebuildModelList()
    {
        std::vector<ModelRegistry::Entry> entries;
        const auto searchText = searchEditor.getText().trim().toLowerCase();

        for (const auto& savedPath : sharedChoices->savedModelPaths)
        {
            const String path(savedPath);

            if (path.startsWithIgnoreCase("click here"))
                continue;

            auto entry = ModelRegistry::getEntryForPath(path);
            const auto searchableText =
                (entry.displayName + " " + entry.summary + " " + entry.path + " " + entry.provider)
                    .toLowerCase();

            if (searchText.isEmpty() || searchableText.contains(searchText))
                entries.push_back(std::move(entry));
        }

        modelList.setEntries(std::move(entries),
                             [this](ModelRegistry::Entry entry) { requestModelLoad(entry); });
        updateListBounds();
    }

    void updateListBounds()
    {
        const auto width = jmax(0, viewport.getWidth() - viewport.getScrollBarThickness());
        modelList.setSize(width, jmax(viewport.getHeight(), modelList.getRequiredHeight()));
    }

    void openCustomPathPopup()
    {
        std::function<void(String)> loadCallback = [this](String path)
        {
            auto entry = ModelRegistry::getEntryForPath(path);
            requestModelLoad(entry);
        };

        auto* content = new CustomPathComponent(std::move(loadCallback), [] {});

        DialogWindow::LaunchOptions options;
        options.dialogTitle = "Enter Custom Path";
        options.dialogBackgroundColour = Colours::darkgrey;
        options.content.setOwned(content);

        options.useNativeTitleBar = false;
        options.resizable = false;
        options.escapeKeyTriggersCloseButton = true;
        options.componentToCentreAround = this;

        options.launchAsync();
    }

    Label titleLabel;
    Label subtitleLabel;
    TextEditor searchEditor;
    TextButton customPathButton;
    Viewport viewport;
    ModelRegistryList modelList;

    SharedResourcePointer<SharedChoices> sharedChoices;
};
