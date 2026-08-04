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

class TagLabel : public Component
{
public:
    TagLabel(const String& text) : tagText(text)
    {
        setSize(getPreferredWidth(), getPreferredHeight());
    }

    int getPreferredWidth() const { return font.getStringWidth(tagText) + horizontalPadding * 2; }
    int getPreferredHeight() const { return roundToInt(font.getHeight()) + verticalPadding * 2; }

    void paint(Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(Colour(0xff183238));
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour(Colour(0xff2dd4bf).withAlpha(0.42f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        g.setColour(Colour(0xff9eeadf));
        g.setFont(font);
        g.drawText(tagText, getLocalBounds(), Justification::centred, true);
    }

private:
    String tagText;
    Font font { 10.0f, Font::bold };
    static constexpr int horizontalPadding = 7;
    static constexpr int verticalPadding = 4;
};

class CategoryChip : public Button
{
public:
    CategoryChip(const String& name, bool selected)
        : Button(name), isSelected(selected)
    {
    }

    void setSelected(bool selected)
    {
        if (isSelected != selected)
        {
            isSelected = selected;
            repaint();
        }
    }

    bool getSelected() const { return isSelected; }

    int getPreferredWidth() const { return font.getStringWidth(getName()) + horizontalPadding * 2; }
    int getPreferredHeight() const { return roundToInt(font.getHeight()) + verticalPadding * 2; }

    void paintButton(Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        
        Colour bg;
        Colour textColour;
        
        if (isSelected)
        {
            bg = Colour(0xff0f766e);
            textColour = Colours::white;
        }
        else if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
        {
            bg = Colour(0xff263a3d);
            textColour = Colours::white;
        }
        else
        {
            bg = Colour(0xff1e1e24);
            textColour = Colours::lightgrey;
        }

        g.setColour(bg);
        g.fillRoundedRectangle(bounds, 5.0f);

        g.setColour(isSelected ? Colour(0xff5eead4) : Colours::white.withAlpha(0.1f));
        g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

        g.setColour(textColour);
        g.setFont(font);
        g.drawText(getName(), getLocalBounds().reduced(horizontalPadding, 0), Justification::centred, true);
    }

private:
    bool isSelected = false;
    Font font { 13.0f, Font::bold };
    static constexpr int horizontalPadding = 12;
    static constexpr int verticalPadding = 7;
};

class CategoryFilterBar : public Component
{
public:
    CategoryFilterBar(std::function<void(String)> onCategorySelectedCallback)
        : onCategorySelected(std::move(onCategorySelectedCallback))
    {
        categories = {
            "All",
            "Generation",
            "Performance Rendering and Synthesis",
            "Effects",
            "Enhancement",
            "Production",
            "Source Separation",
            "Analysis",
            "Custom"
        };

        for (int i = 0; i < categories.size(); ++i)
        {
            auto chip = std::make_unique<CategoryChip>(categories[i], i == 0);
            chip->onClick = [this, category = categories[i]]
            {
                selectCategory(category);
            };
            addAndMakeVisible(*chip);
            chips.push_back(std::move(chip));
        }
    }

    void selectCategory(const String& category)
    {
        for (auto& chip : chips)
        {
            chip->setSelected(chip->getName() == category);
        }

        if (onCategorySelected)
            onCategorySelected(category);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        int x = 0;
        int y = 0;
        int spacingX = 6;
        int spacingY = 6;
        int rowHeight = 0;

        for (auto& chip : chips)
        {
            int chipWidth = chip->getPreferredWidth();
            int chipHeight = chip->getPreferredHeight();
            
            if (x + chipWidth > area.getWidth() && x > 0)
            {
                x = 0;
                y += rowHeight + spacingY;
                rowHeight = 0;
            }

            chip->setBounds(x, y, chipWidth, chipHeight);
            x += chipWidth + spacingX;
            rowHeight = jmax(rowHeight, chipHeight);
        }
        
        int newHeight = y + jmax(rowHeight, 1);
        if (newHeight != preferredHeight)
        {
            preferredHeight = newHeight;
            MessageManager::callAsync([this]()
            {
                if (auto* parent = getParentComponent())
                    parent->resized();
            });
        }
    }

    int getPreferredHeight() const { return preferredHeight; }

private:
    std::vector<String> categories;
    std::vector<std::unique_ptr<CategoryChip>> chips;
    std::function<void(String)> onCategorySelected;
    int preferredHeight = 28;
};

class CategoryHeader : public Component
{
public:
    CategoryHeader(const String& name) : categoryName(name) {}

    void paint(Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        
        g.setColour(Colours::white);
        g.setFont(Font(16.0f, Font::bold));
        g.drawText(categoryName, getLocalBounds().reduced(4, 0), Justification::centredLeft, true);
        
        auto textWidth = Font(16.0f, Font::bold).getStringWidth(categoryName);
        g.setColour(Colour(0xff2dd4bf).withAlpha(0.6f));
        g.fillRect(textWidth + 12.0f, bounds.getCentreY() - 1.0f, bounds.getWidth() - textWidth - 16.0f, 2.0f);
    }

    static constexpr int preferredHeight = 32;

private:
    String categoryName;
};

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

        for (const auto& tag : entry.tags)
        {
            auto label = std::make_unique<TagLabel>(tag);
            addAndMakeVisible(*label);
            tagLabels.push_back(std::move(label));
        }
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

        auto topRow = area.removeFromTop(18);
        providerLabel.setBounds(topRow.removeFromLeft(150));
        
        for (auto& tagLabel : tagLabels)
        {
            tagLabel->setBounds(topRow.removeFromRight(tagLabel->getPreferredWidth() + 4)
                                      .withSizeKeepingCentre(tagLabel->getPreferredWidth(),
                                                             tagLabel->getPreferredHeight()));
        }

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
    std::vector<std::unique_ptr<TagLabel>> tagLabels;
};

class ModelRegistryList : public Component
{
public:
    struct Section
    {
        String category;
        std::vector<ModelRegistry::Entry> entries;
    };

    void setSections(std::vector<Section> newSections,
                     std::function<void(ModelRegistry::Entry)> loadCallback)
    {
        items.clear();
        removeAllChildren();

        for (auto& sec : newSections)
        {
            if (sec.entries.empty())
                continue;

            auto header = std::make_unique<CategoryHeader>(sec.category);
            addAndMakeVisible(*header);
            items.push_back(std::move(header));

            for (auto& entry : sec.entries)
            {
                auto card = std::make_unique<ModelRegistryCard>(std::move(entry), loadCallback);
                addAndMakeVisible(*card);
                items.push_back(std::move(card));
            }
        }

        resized();
        repaint();
    }

    void resized() override
    {
        auto area = getLocalBounds();

        for (auto& item : items)
        {
            if (dynamic_cast<CategoryHeader*>(item.get()))
                item->setBounds(area.removeFromTop(CategoryHeader::preferredHeight));
            else if (dynamic_cast<ModelRegistryCard*>(item.get()))
                item->setBounds(area.removeFromTop(ModelRegistryCard::preferredHeight).reduced(0, 4));
        }
    }

    int getRequiredHeight() const
    {
        int height = 0;
        for (const auto& item : items)
        {
            if (dynamic_cast<CategoryHeader*>(item.get()))
                height += CategoryHeader::preferredHeight;
            else if (dynamic_cast<ModelRegistryCard*>(item.get()))
                height += ModelRegistryCard::preferredHeight;
        }
        return height;
    }

private:
    std::vector<std::unique_ptr<Component>> items;
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
        addAndMakeVisible(categoryFilterBar);
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
        categoryFilterBar.setBounds(area.removeFromTop(categoryFilterBar.getPreferredHeight()));

        area.removeFromTop(10);
        viewport.setBounds(area);

        updateListBounds();
    }

    void resetSelection()
    {
        searchEditor.setEnabled(true);
        customPathButton.setEnabled(true);
        categoryFilterBar.setEnabled(true);
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
        categoryFilterBar.setEnabled(false);
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
            {
                if (activeCategory == "All")
                {
                    entries.push_back(std::move(entry));
                }
                else if (activeCategory == "Custom")
                {
                    if (entry.tags.empty())
                        entries.push_back(std::move(entry));
                }
                else
                {
                    bool matchesCategory = false;
                    for (const auto& tag : entry.tags)
                    {
                        if (tag == activeCategory)
                        {
                            matchesCategory = true;
                            break;
                        }
                    }
                    if (matchesCategory)
                        entries.push_back(std::move(entry));
                }
            }
        }

        std::vector<ModelRegistryList::Section> sections;
        std::vector<String> categoriesToShow;
        
        if (activeCategory == "All")
        {
            categoriesToShow = {
                "Generation",
                "Performance Rendering and Synthesis",
                "Effects",
                "Enhancement",
                "Production",
                "Source Separation",
                "Analysis",
                "Custom"
            };
        }
        else
        {
            categoriesToShow = { activeCategory };
        }

        for (const auto& cat : categoriesToShow)
        {
            ModelRegistryList::Section sec;
            sec.category = cat;
            
            for (const auto& entry : entries)
            {
                if (cat == "Custom")
                {
                    if (entry.tags.empty())
                        sec.entries.push_back(entry);
                }
                else
                {
                    for (const auto& tag : entry.tags)
                    {
                        if (tag == cat)
                        {
                            sec.entries.push_back(entry);
                            break;
                        }
                    }
                }
            }
            
            if (! sec.entries.empty())
                sections.push_back(std::move(sec));
        }

        modelList.setSections(std::move(sections),
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
    CategoryFilterBar categoryFilterBar { [this](String cat) { activeCategory = cat; rebuildModelList(); } };
    String activeCategory { "All" };
    Viewport viewport;
    ModelRegistryList modelList;

    SharedResourcePointer<SharedChoices> sharedChoices;
};
