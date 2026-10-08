/**
 * @file MultiSelectWithLabel.h
 * @brief Custom multiple-selection dropdown component with label.
 * @author cwitkowitz
 */

#pragma once

#include <algorithm>

#include "ControlComponent.h"

using namespace juce;

/**
 * JUCE has no multiple-selection combo box, so this presents the options as a
 * popup menu of tickable items and summarizes the selection on the button.
 */
class MultiSelectWithLabel : public ControlComponent
{
public:
    MultiSelectWithLabel(const String& labelText = {})
    {
        label.setText(labelText, dontSendNotification);
        label.setJustificationType(Justification::centred);

        selectionButton.onClick = [this] { showSelectionMenu(); };

        addAndMakeVisible(label);
        addAndMakeVisible(selectionButton);
    }

    void resized() override
    {
        auto area = getLocalBounds();

        if (area.isEmpty())
        {
            return;
        }

        label.setBounds(area.removeFromTop(jmin(labelHeight, area.getHeight())));
        selectionButton.setBounds(area);

        // What fits in the button depends on its width, so recompute the summary
        updateSelectionText();
    }

    int getPreferredWidth() const override { return preferredSelectionWidth; }

    int getPreferredHeight() const override { return preferredSelectionHeight; }

    int getMinimumRequiredWidth() const override
    {
        const int labelWidth = getLabelWidth(label);
        return jmax(minSelectionWidth, labelWidth + defaultPadding);
    }

    /** Receives notification when the selection is changed by the user. */
    struct Listener
    {
        virtual ~Listener() = default;

        virtual void multiSelectChanged(MultiSelectWithLabel* multiSelect) = 0;
    };

    void addListener(Listener* listener) { listeners.add(listener); }

    void removeListener(Listener* listener) { listeners.remove(listener); }

    void setOptions(const std::vector<String>& newOptions)
    {
        options = newOptions;

        updateSelectionText();
    }

    /** Sets the selection without notifying listeners, as JUCE controls do. */
    void setSelection(const std::vector<String>& newSelection)
    {
        selected = newSelection;

        updateSelectionText();
    }

    const std::vector<String>& getSelection() const { return selected; }

    bool isSelected(const String& option) const
    {
        return std::find(selected.begin(), selected.end(), option) != selected.end();
    }

    TextButton& getSelectionButton() { return selectionButton; }

private:
    void updateSelectionText()
    {
        StringArray chosen;

        for (const auto& option : options)
        {
            if (isSelected(option))
            {
                chosen.add(option);
            }
        }

        const String allSelected = chosen.joinIntoString(", ");

        if (chosen.isEmpty())
        {
            selectionButton.setButtonText("None selected");
        }
        else
        {
            /* Name the choices rather than counting them, falling back to a count
               only when the list cannot fit in the button */
            const Font font = selectionButton.getLookAndFeel().getTextButtonFont(
                selectionButton, selectionButton.getHeight());

            const int available = selectionButton.getWidth() - textPadding;

            if (available <= 0 || font.getStringWidth(allSelected) <= available)
            {
                selectionButton.setButtonText(allSelected);
            }
            else
            {
                selectionButton.setButtonText(String(chosen.size()) + " selected");
            }
        }

        selectionButton.setTooltip(allSelected);
    }

    void showSelectionMenu()
    {
        PopupMenu menu;

        for (int i = 0; i < (int) options.size(); ++i)
        {
            const auto& option = options[(size_t) i];
            const bool ticked = isSelected(option);

            // Item IDs are 1-based, so that 0 can signal a dismissed menu
            menu.addItem(i + 1, option, true, ticked);
        }

        // The menu outlives this component if the tab is torn down while it is open
        Component::SafePointer<MultiSelectWithLabel> safeThis(this);

        menu.showMenuAsync(PopupMenu::Options()
                               .withTargetComponent(&selectionButton)
                               .withMinimumWidth(selectionButton.getWidth()),
                           [safeThis](int result)
                           {
                               if (safeThis == nullptr || result == 0)
                               {
                                   return;
                               }

                               safeThis->toggle(safeThis->options[(size_t) (result - 1)]);
                           });
    }

    void toggle(const String& option)
    {
        auto existing = std::find(selected.begin(), selected.end(), option);

        if (existing == selected.end())
        {
            selected.push_back(option);
        }
        else
        {
            selected.erase(existing);
        }

        updateSelectionText();

        listeners.call([this](Listener& l) { l.multiSelectChanged(this); });
    }

    static constexpr int preferredSelectionWidth = 140;
    static constexpr int preferredSelectionHeight = 44;

    static constexpr int minSelectionWidth = 120;
    static constexpr int labelHeight = 20;
    static constexpr int textPadding = 16;

    std::vector<String> options;
    std::vector<String> selected;

    ListenerList<Listener> listeners;

    Label label;
    TextButton selectionButton;
};
