/**
 * @file MultiSelectWithLabel.h
 * @brief Custom multiple-selection dropdown component with label.
 * @author cwitkowitz
 */

#pragma once

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
        auto selectionArea = getLocalBounds();
        auto labelArea = selectionArea.removeFromTop(20);

        label.setBounds(labelArea);
        selectionButton.setBounds(selectionArea);
    }

    int getMinimumRequiredWidth() const override
    {
        const int labelWidth = getLabelWidth(label);
        return jmax(minSelectionWidth, labelWidth + defaultPadding);
    }

    void setOptions(const std::vector<String>& newOptions) { options = newOptions; }

    /** Returns whether the given option is currently selected. */
    std::function<bool(const String&)> isOptionSelected;

    /** Called when an option is ticked or unticked. */
    std::function<void(const String&, bool)> onOptionToggled;

    void updateSelectionText()
    {
        StringArray selected;

        for (const auto& option : options)
        {
            if (isOptionSelected && isOptionSelected(option))
            {
                selected.add(option);
            }
        }

        if (selected.isEmpty())
        {
            selectionButton.setButtonText("None selected");
        }
        else if (selected.size() == 1)
        {
            selectionButton.setButtonText(selected[0]);
        }
        else
        {
            selectionButton.setButtonText(String(selected.size()) + " selected");
        }

        selectionButton.setTooltip(selected.joinIntoString(", "));
    }

    TextButton& getSelectionButton() { return selectionButton; }

private:
    void showSelectionMenu()
    {
        PopupMenu menu;

        for (int i = 0; i < (int) options.size(); ++i)
        {
            const auto& option = options[(size_t) i];
            const bool ticked = isOptionSelected && isOptionSelected(option);

            // Item IDs are 1-based, so that 0 can signal a dismissed menu
            menu.addItem(i + 1, option, true, ticked);
        }

        // The menu outlives this component if the tab is torn down while it is open
        Component::SafePointer<MultiSelectWithLabel> safeThis(this);

        menu.showMenuAsync(
            PopupMenu::Options().withTargetComponent(&selectionButton),
            [safeThis](int result)
            {
                if (safeThis == nullptr || result == 0)
                {
                    return;
                }

                const auto& option = safeThis->options[(size_t) (result - 1)];

                const bool wasSelected =
                    safeThis->isOptionSelected && safeThis->isOptionSelected(option);

                if (safeThis->onOptionToggled)
                {
                    safeThis->onOptionToggled(option, ! wasSelected);
                }

                safeThis->updateSelectionText();
            });
    }

    static constexpr int minSelectionWidth = 120;

    std::vector<String> options;

    Label label;
    TextButton selectionButton;
};
