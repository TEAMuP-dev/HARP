/**
 * @file HomeTab.h
 * @brief Home tab for browsing the model catalog and opening models in new tabs.
 * @author JEYuhas, 2cylu2, VedMistry42, cwitkowitz
 */

#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "gui/HoverHandler.h"

#include "utils/Interface.h"
#include "utils/Messages.h"
#include "utils/ModelCatalog.h"
#include "utils/ModelTags.h"

#include "widgets/ModelStyle.h"

#include "windows/tutorial/TutorialTargets.h"

#include "windows/CustomPathWindow.h"

using namespace juce;

/**
 * Clickable card summarizing one catalog entry.
 */
class ModelCard : public Button
{
public:
    ModelCard(const CatalogEntry& catalogEntry, std::function<void(const CatalogEntry&)> onOpen)
        : Button(catalogEntry.name),
          entry(catalogEntry),
          badges(ModelStyle::getBadges(catalogEntry)),
          tagLabels(catalogEntry.tags.getDisplayLabels()),
          searchableText(catalogEntry.getSearchableText()),
          onOpenRequested(std::move(onOpen))
    {
        setMouseCursor(MouseCursor::PointingHandCursor);
    }

    const CatalogEntry& getEntry() const { return entry; }

    bool matchesSearch(const String& lowercaseQuery) const
    {
        return lowercaseQuery.isEmpty() || searchableText.contains(lowercaseQuery);
    }

    // Removing a custom path is offered from the context menu
    std::function<void(const CatalogEntry&)> onRemoveRequested;

    // Opens the model on a click, or offers to remove a custom path on a right-click
    using Button::clicked;
    void clicked(const ModifierKeys& modifiers) override
    {
        if (! modifiers.isPopupMenu())
        {
            if (onOpenRequested)
                onOpenRequested(entry);

            return;
        }

        if (entry.isCustom && onRemoveRequested)
        {
            PopupMenu menu;
            menu.addItem("Remove from list",
                         [safeThis = SafePointer<ModelCard>(this)]
                         {
                             if (safeThis != nullptr)
                                 safeThis->onRemoveRequested(safeThis->entry);
                         });
            menu.showMenuAsync(PopupMenu::Options().withTargetComponent(this));
        }
    }

    // Everything the card has no room for is shown in the instructions box while hovering
    void mouseEnter(const MouseEvent& e) override
    {
        Button::mouseEnter(e);

        StringArray lines;

        if (entry.description.isNotEmpty())
            lines.add(entry.description);

        if (const String tags = ModelStyle::describeTags(entry.tags); tags.isNotEmpty())
            lines.add(tags);

        lines.add("Click to open " + entry.path + " in a new tab."
                  + (entry.isCustom ? " Right-click to remove it from the list." : ""));

        instructionsMessage->setMessage(lines.joinIntoString("\n"));
    }

    void mouseExit(const MouseEvent& e) override
    {
        Button::mouseExit(e);
        instructionsMessage->clearMessage();
    }

    void paintButton(Graphics& g, bool isHighlighted, bool isDown) override
    {
        using namespace ModelStyle;

        drawCard(g, getLocalBounds().toFloat().reduced(1.0f), isHighlighted, isDown);

        auto area = getLocalBounds().reduced(12, 9);

        /* Name, with badges on the right */

        auto nameRow = area.removeFromTop(20);
        const int badgesLeft = drawBadges(g, badges, nameRow);

        g.setColour(Colours::white);
        g.setFont(font(15.0f, true));
        g.drawText(
            entry.name, nameRow.withRight(badgesLeft - chipGap), Justification::centredLeft, true);

        /* Description, cut short (all of it is shown while hovering) */

        area.removeFromTop(2);

        g.setColour(Colours::whitesmoke.withAlpha(0.85f));
        g.setFont(font(13.0f));
        drawTruncatedText(g,
                          entry.description.isNotEmpty() ? entry.description
                                                         : String("No description provided."),
                          area.removeFromTop(30),
                          2);

        /* Tags, as many as fit (all of them are described while hovering) */

        area.removeFromTop(4);
        drawTagRow(g, tagLabels, area.removeFromTop(chipHeight));

        /* Path */

        area.removeFromTop(4);

        g.setColour(Colours::grey);
        g.setFont(font(11.0f));
        g.drawText(entry.path, area.removeFromTop(13), Justification::centredLeft, true);
    }

    static constexpr int preferredHeight = 110;
    static constexpr int minimumWidth = 300;

private:
    const CatalogEntry entry;
    const std::vector<ModelStyle::Badge> badges;
    const StringArray tagLabels;
    const String searchableText;

    std::function<void(const CatalogEntry&)> onOpenRequested;

    SharedResourcePointer<InstructionsMessage> instructionsMessage;
};

/**
 * The catalog as a responsive grid of cards, in one section per category.
 */
class ModelGrid : public Component
{
public:
    // Stands for models that declare no category
    static inline const String otherSectionId { "other" };

    std::function<void(const CatalogEntry&)> onOpenRequested;
    std::function<void(const CatalogEntry&)> onRemoveRequested;

    void setEntries(const std::vector<CatalogEntry>& entries)
    {
        sections.clear();
        removeAllChildren();

        auto addSection = [&](const String& id, const String& title)
        {
            auto section = std::make_unique<Section>();
            section->id = id;
            section->title = title;

            for (const auto& entry : entries)
            {
                const bool belongs = id == otherSectionId ? ! entry.tags.isCategorized()
                                                              : entry.tags.isInCategory(id);

                if (! belongs)
                    continue;

                auto* card = section->cards.add(new ModelCard(entry, onOpenRequested));
                card->onRemoveRequested = onRemoveRequested;
                addChildComponent(card);
            }

            // A model that belongs to several categories is listed in each of them
            sections.push_back(std::move(section));
        };

        for (const auto& category : Taxonomy::getCategories())
            addSection(category.id, category.displayName);

        addSection(otherSectionId, "Other");
    }

    // Number of models in a section, or across all sections for an empty id
    int countEntries(const String& sectionId) const
    {
        StringArray paths;

        for (const auto& section : sections)
        {
            if (sectionId.isNotEmpty() && section->id != sectionId)
                continue;

            for (auto* card : section->cards)
                paths.addIfNotAlreadyThere(card->getEntry().path);
        }

        return paths.size();
    }

    /**
     * Shows the cards in a section (or all sections, for an empty id) that match a
     * search, and returns how many distinct models are shown. The grid has to be laid out
     * again afterwards.
     */
    int applyFilter(const String& sectionId, const String& searchText)
    {
        const String query = searchText.trim().toLowerCase();

        StringArray shownPaths;

        for (auto& section : sections)
        {
            const bool sectionShown = sectionId.isEmpty() || section->id == sectionId;

            for (auto* card : section->cards)
            {
                const bool shown = sectionShown && card->matchesSearch(query);
                card->setVisible(shown);

                if (shown)
                    shownPaths.addIfNotAlreadyThere(card->getEntry().path);
            }
        }

        return shownPaths.size();
    }

    int getHeightForWidth(int width) const { return layOut(width, false); }

    void resized() override { layOut(getWidth(), true); }

    void paint(Graphics& g) override
    {
        for (const auto& section : sections)
        {
            if (! section->headerBounds.isEmpty())
                ModelStyle::drawSectionHeader(g, section->title, section->headerBounds);
        }
    }

private:
    struct Section
    {
        String id;
        String title;
        OwnedArray<ModelCard> cards;
        Rectangle<int> headerBounds;
    };

    // Lays the visible cards out in as many columns as fit, and returns the height used
    int layOut(int width, bool applyBounds) const
    {
        const int columns = jmax(1, (width + gap) / (ModelCard::minimumWidth + gap));
        const int cardWidth = jmax(0, (width - gap * (columns - 1)) / columns);

        int y = 0;

        for (const auto& section : sections)
        {
            Array<ModelCard*> visibleCards;

            for (auto* card : section->cards)
            {
                if (card->isVisible())
                    visibleCards.add(card);
            }

            if (visibleCards.isEmpty())
            {
                if (applyBounds)
                    section->headerBounds = {};

                continue;
            }

            if (applyBounds)
                section->headerBounds = { 0, y, width, ModelStyle::sectionHeaderHeight };

            y += ModelStyle::sectionHeaderHeight;

            for (int i = 0; i < visibleCards.size(); ++i)
            {
                const int column = i % columns;

                if (column == 0 && i > 0)
                    y += ModelCard::preferredHeight + gap;

                if (applyBounds)
                    visibleCards[i]->setBounds(
                        column * (cardWidth + gap), y, cardWidth, ModelCard::preferredHeight);
            }

            y += ModelCard::preferredHeight + sectionGap;
        }

        return y;
    }

    static constexpr int gap = 8;
    static constexpr int sectionGap = 10;

    std::vector<std::unique_ptr<Section>> sections;
};

/**
 * Chips for choosing which section of the catalog to show, wrapped onto as many rows as
 * the width requires.
 */
class CategoryFilterBar : public Component
{
public:
    std::function<void()> onSelectionChanged;

    CategoryFilterBar()
    {
        addChip({}, "All");

        for (const auto& category : Taxonomy::getCategories())
            addChip(category.id, category.displayName);

        addChip(ModelGrid::otherSectionId, "Other");

        chips.getFirst()->setToggleState(true, dontSendNotification);
    }

    // Id of the selected section, or empty for all of them
    String getSelectedId() const
    {
        for (auto* chip : chips)
        {
            if (chip->getToggleState())
                return chip->id;
        }

        return {};
    }

    void setCounts(const ModelGrid& grid)
    {
        for (auto* chip : chips)
            chip->count = grid.countEntries(chip->id);

        resized();
        repaint();
    }

    int getHeightForWidth(int width) const
    {
        FlexBox box = createLayout();
        box.performLayout(Rectangle<int>(0, 0, width, 1000));

        float bottom = 0.0f;

        for (const auto& item : box.items)
            bottom = jmax(bottom, item.currentBounds.getBottom() + item.margin.bottom);

        return roundToInt(bottom);
    }

    void resized() override { createLayout().performLayout(getLocalBounds()); }

private:
    struct Chip : public Button
    {
        Chip(const String& chipId, const String& chipName) : Button(chipName), id(chipId)
        {
            setClickingTogglesState(true);
            setRadioGroupId(1);
        }

        String getText() const { return getName() + "  " + String(count); }

        int getPreferredWidth() const
        {
            return ModelStyle::getTextWidth(chipFont, getText()) + 2 * horizontalPadding;
        }

        void paintButton(Graphics& g, bool isHighlighted, bool isDown) override
        {
            const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
            const bool selected = getToggleState();

            g.setColour(selected ? ModelStyle::accentDark
                                 : Colour(isHighlighted || isDown ? 0xff263a3d : 0xff1e1e24));
            g.fillRoundedRectangle(bounds, 5.0f);

            g.setColour(selected ? ModelStyle::accent : Colours::white.withAlpha(0.1f));
            g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

            g.setColour(selected || isHighlighted ? Colours::white : Colours::lightgrey);
            g.setFont(chipFont);
            g.drawText(getText(), getLocalBounds(), Justification::centred, false);
        }

        void mouseEnter(const MouseEvent& e) override
        {
            Button::mouseEnter(e);

            if (id.isEmpty())
                instructionsMessage->setMessage("Click to show every model.");
            else if (id == ModelGrid::otherSectionId)
                instructionsMessage->setMessage(
                    "Click to show the models that declare no category.");
            else
                instructionsMessage->setMessage("Click to show only the " + getName() + " models.");
        }

        void mouseExit(const MouseEvent& e) override
        {
            Button::mouseExit(e);
            instructionsMessage->clearMessage();
        }

        const String id;
        int count = 0;

        const Font chipFont = ModelStyle::font(12.0f, true);
        static constexpr int horizontalPadding = 10;

        SharedResourcePointer<InstructionsMessage> instructionsMessage;
    };

    void addChip(const String& id, const String& name)
    {
        auto* chip = chips.add(new Chip(id, name));
        chip->onClick = [this]
        {
            if (onSelectionChanged)
                onSelectionChanged();
        };
        addAndMakeVisible(chip);
    }

    FlexBox createLayout() const
    {
        FlexBox box;
        box.flexWrap = FlexBox::Wrap::wrap;
        box.alignContent = FlexBox::AlignContent::flexStart;

        for (auto* chip : chips)
        {
            box.items.add(FlexItem(*chip)
                              .withWidth((float) chip->getPreferredWidth())
                              .withHeight((float) chipHeight)
                              .withMargin(FlexItem::Margin(0, 6, 6, 0)));
        }

        return box;
    }

    static constexpr int chipHeight = 26;

    OwnedArray<Chip> chips;
};

/**
 * A small note on the state of the catalog (e.g., "Offline" or "2 hidden"), shown only when
 * there is something to note, and explained in the instructions box while hovering over it.
 */
class CatalogStatusIndicator : public Component
{
public:
    void setStatus(const String& newText, Colour newColour, const String& newDetails)
    {
        text = newText;
        colour = newColour;
        details = newDetails;

        setVisible(text.isNotEmpty());
        repaint();
    }

    int getPreferredWidth() const
    {
        return text.isEmpty() ? 0 : ModelStyle::getTextWidth(textFont, text) + 16;
    }

    void paint(Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced(0.5f, 3.0f);

        g.setColour(colour.withAlpha(0.18f));
        g.fillRoundedRectangle(bounds, bounds.getHeight() / 2.0f);

        g.setColour(colour);
        g.setFont(textFont);
        g.drawText(text, getLocalBounds(), Justification::centred, false);
    }

    void mouseEnter(const MouseEvent&) override { instructionsMessage->setMessage(details); }

    void mouseExit(const MouseEvent&) override { instructionsMessage->clearMessage(); }

private:
    String text;
    Colour colour;
    String details;

    const Font textFont = ModelStyle::font(12.0f, true);

    SharedResourcePointer<InstructionsMessage> instructionsMessage;
};

class HomeTab : public Component, private ChangeListener
{
public:
    HomeTab()
    {
        titleLabel.setText("Models", dontSendNotification);
        titleLabel.setFont(ModelStyle::font(20.0f, true));
        titleLabel.setBorderSize({ 0, 0, 0, 0 });
        addAndMakeVisible(titleLabel);

        addChildComponent(statusIndicator);

        addInstructions(refreshButton,
                        "Click to fetch the latest list of models from Hugging Face.");
        refreshButton.onClick = [this] { catalog->refresh(); };
        addAndMakeVisible(refreshButton);

        addInstructions(customPathButton,
                        "Click to open a model that is not listed, by its Hugging Face Space, "
                        "Gradio URL, or local address.");
        customPathButton.onClick = [this]
        {
            CustomPathComponent::launch(this,
                                        [safeThis = SafePointer<HomeTab>(this)](String path)
                                        {
                                            if (safeThis != nullptr)
                                                safeThis->requestOpen(path, {});
                                        });
        };
        addAndMakeVisible(customPathButton);

        searchEditor.setTextToShowWhenEmpty("Search by name, task, tag, or path...", Colours::grey);
        searchEditor.setMultiLine(false);
        searchEditor.setReturnKeyStartsNewLine(false);
        searchEditor.onTextChange = [this] { applyFilter(); };
        addInstructions(searchEditor,
                        "Type to show only the models whose name, description, tags, or path "
                        "contain the text.");
        addAndMakeVisible(searchEditor);

        categoryFilterBar.onSelectionChanged = [this] { applyFilter(); };
        addAndMakeVisible(categoryFilterBar);

        modelGrid.onOpenRequested = [this](const CatalogEntry& entry)
        { requestOpen(entry.path, entry.name); };
        modelGrid.onRemoveRequested = [this](const CatalogEntry& entry)
        { catalog->removeCustomPath(entry.path); };

        viewport.setViewedComponent(&modelGrid, false);
        viewport.setScrollBarsShown(true, false);

        searchEditor.setComponentID(TutorialTargets::modelSearch);
        viewport.setComponentID(TutorialTargets::modelList);
        addAndMakeVisible(viewport);

        noResultsLabel.setJustificationType(Justification::centredTop);
        noResultsLabel.setColour(Label::textColourId, Colours::grey);
        addChildComponent(noResultsLabel);

        catalog->addChangeListener(this);
        updateFromCatalog();
    }

    ~HomeTab() override
    {
        catalog->removeChangeListener(this);

        for (auto& handler : hoverHandlers)
            handler->detach();
    }

    // Called with the path and display name of the model to open
    std::function<void(const String&, const String&)> onModelOpenRequested;

    void resized() override
    {
        auto area = getLocalBounds().reduced(14, 12);

        FlexBox header;
        header.alignItems = FlexBox::AlignItems::center;
        header.items.add(FlexItem(titleLabel).withFlex(1).withHeight(28));
        header.items.add(FlexItem(statusIndicator)
                             .withWidth((float) statusIndicator.getPreferredWidth())
                             .withHeight(26)
                             .withMargin({ 0, 8, 0, 0 }));
        header.items.add(
            FlexItem(refreshButton).withWidth(80).withHeight(26).withMargin({ 0, 6, 0, 0 }));
        header.items.add(FlexItem(customPathButton).withWidth(110).withHeight(26));
        header.performLayout(area.removeFromTop(28));

        area.removeFromTop(8);
        searchEditor.setBounds(area.removeFromTop(28));

        area.removeFromTop(8);
        categoryFilterBar.setBounds(
            area.removeFromTop(categoryFilterBar.getHeightForWidth(area.getWidth())));

        viewport.setBounds(area);
        noResultsLabel.setBounds(area.withTrimmedTop(24));

        layOutGrid();
    }

private:
    void changeListenerCallback(ChangeBroadcaster*) override { updateFromCatalog(); }

    void updateFromCatalog()
    {
        modelGrid.setEntries(catalog->getEntries());
        categoryFilterBar.setCounts(modelGrid);

        updateStatusIndicator();
        refreshButton.setEnabled(catalog->getFetchState() != ModelCatalog::FetchState::Fetching);

        // The chip counts, and so the rows they wrap onto, may have changed as well
        updateVisibleModels();
        resized();
    }

    void updateStatusIndicator()
    {
        const String source = "huggingface.co/" + String(ModelCatalog::hubOrganization);
        const auto& hidden = catalog->getHiddenEntries();

        StringArray notes;
        StringArray details;
        Colour colour = ModelStyle::secondaryText;

        switch (catalog->getFetchState())
        {
            case ModelCatalog::FetchState::Fetching:
                notes.add("Updating...");
                details.add("Fetching the latest list of models from " + source + ".");
                colour = ModelStyle::information;
                break;

            case ModelCatalog::FetchState::Failed:
            {
                const Time listingTime = catalog->getListingTime();

                notes.add("Offline");
                details.add("Could not reach " + source + " (" + catalog->getFetchError() + "). "
                            + (listingTime != Time()
                                   ? "Showing the list from "
                                         + listingTime.toString(true, true, false) + "."
                                   : String("Only built-in and custom models are shown."))
                            + " Click Refresh to try again.");
                colour = ModelStyle::problem;
                break;
            }

            case ModelCatalog::FetchState::Succeeded:
                break;
        }

        if (! hidden.empty())
        {
            notes.add(String((int) hidden.size()) + " hidden");

            StringArray hiddenLines { "Hidden, since they cannot currently be loaded:" };

            for (const auto& entry : hidden)
                hiddenLines.add(entry.name + " (" + entry.reason + ")");

            details.add(hiddenLines.joinIntoString("\n"));
        }

        statusIndicator.setStatus(notes.joinIntoString(String::fromUTF8(" \xc2\xb7 ")),
                                  colour,
                                  details.joinIntoString("\n"));
    }

    void applyFilter()
    {
        updateVisibleModels();
        layOutGrid();
    }

    void updateVisibleModels()
    {
        const int numShown =
            modelGrid.applyFilter(categoryFilterBar.getSelectedId(), searchEditor.getText());

        noResultsLabel.setText(searchEditor.isEmpty()
                                   ? "No models in this category yet."
                                   : "No models match \"" + searchEditor.getText().trim() + "\".",
                               dontSendNotification);
        noResultsLabel.setVisible(numShown == 0);
    }

    void layOutGrid()
    {
        const int width = jmax(0, viewport.getWidth() - viewport.getScrollBarThickness());
        const int height = jmax(viewport.getHeight(), modelGrid.getHeightForWidth(width));

        // Which cards are shown can change without the size changing
        if (modelGrid.getWidth() == width && modelGrid.getHeight() == height)
            modelGrid.resized();
        else
            modelGrid.setSize(width, height);

        modelGrid.repaint();
    }

    void requestOpen(const String& path, const String& name)
    {
        if (onModelOpenRequested)
            onModelOpenRequested(path, name);
    }

    // Shows instructions for a component in the instructions box while hovering over it
    void addInstructions(Component& component, const String& instructions)
    {
        auto handler = std::make_unique<HoverHandler>(component);

        handler->onMouseEnter = [this, instructions]
        { instructionsMessage->setMessage(instructions); };
        handler->onMouseExit = [this] { instructionsMessage->clearMessage(); };
        handler->attach();

        hoverHandlers.push_back(std::move(handler));
    }

    Label titleLabel;
    CatalogStatusIndicator statusIndicator;
    TextButton refreshButton { "Refresh" };
    TextButton customPathButton { "Custom Path..." };
    TextEditor searchEditor;
    CategoryFilterBar categoryFilterBar;
    Viewport viewport;
    ModelGrid modelGrid;
    Label noResultsLabel;

    std::vector<std::unique_ptr<HoverHandler>> hoverHandlers;

    SharedResourcePointer<ModelCatalog> catalog;
    SharedResourcePointer<InstructionsMessage> instructionsMessage;
};
