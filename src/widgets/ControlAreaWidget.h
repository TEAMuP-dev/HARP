/**
 * @file ControlAreaWidget.h
 * @brief Component comprising all model controls.
 * @author cwitkowitz, saumya-pailwan, hugofloresgarcia, xribene
 */

#pragma once

#include <array>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../widgets/StatusAreaWidget.h"

#include "../gui/ComboBoxWithLabel.h"
#include "../gui/FileChooserWithLabel.h"
#include "../gui/HoverHandler.h"
#include "../gui/MultiSelectWithLabel.h"
#include "../gui/NumberBoxWithLabel.h"
#include "../gui/SliderWithLabel.h"
#include "../gui/TextBoxWithLabel.h"
#include "../gui/ToggleWithLabel.h"

#include "../utils/Controls.h"
#include "../utils/Logging.h"

using namespace juce;

class ControlAreaWidget : public Component
{
public:
    ~ControlAreaWidget() override { resetState(); }

    void resized() override
    {
        auto area = getLocalBounds().reduced(marginSize);

        auto rows = buildRowsForWidth(area.getWidth());

        if (area.isEmpty() || rows.empty())
        {
            return;
        }

        int y = area.getY();

        for (const auto& row : rows)
        {
            int totalComponentWidth = 0;

            for (const auto& entry : row)
            {
                totalComponentWidth += entry.width;
            }

            int baseSpacing = minInterItemGap * (int) (row.size() - 1);
            int remaining = area.getWidth() - totalComponentWidth - baseSpacing;

            int distributed = jmax(0, remaining / ((int) row.size() + 1));

            int edgeGap = minEdgeGap + distributed;
            int slotGap = minInterItemGap + distributed;

            int rowHeight = getRowHeight(row);

            int x = area.getX() + edgeGap;

            for (const auto& entry : row)
            {
                int componentY = y + (rowHeight - entry.height) / 2;
                entry.component->setBounds(x, componentY, entry.width, entry.height);
                x += entry.width + slotGap;
            }

            y += rowHeight + minRowGap;
        }
    }

    int getNumControls() const { return (int) controls.size(); }

    int getMinimumRequiredWidth() const
    {
        int requiredWidth = 0;

        for (const auto& c : controls)
        {
            requiredWidth = jmax(requiredWidth, c->getMinimumRequiredWidth());
        }

        return requiredWidth + 2 * (marginSize + minEdgeGap);
    }

    int getRequiredHeightForWidth(int width) const
    {
        auto rows = buildRowsForWidth(width - 2 * marginSize);

        if (rows.empty())
        {
            return 0;
        }

        int totalHeight = 2 * marginSize + minRowGap * (int) (rows.size() - 1);

        for (const auto& row : rows)
        {
            totalHeight += getRowHeight(row);
        }

        return totalHeight;
    }

    void resetState()
    {
        for (auto& c : controls)
        {
            removeChildComponent(c.get());
        }

        controls.clear();
        handlers.clear();
    }

    void updateControls(const ModelComponentInfoList& controlsInfo)
    {
        resetState();

        /* Controls are laid out grouped by kind, in this order, rather than in the order
           the model declares them */
        std::array<std::vector<std::unique_ptr<ControlComponent>>, 7> groups;

        for (const auto& info : controlsInfo)
        {
            if (auto* sliderInfo = dynamic_cast<SliderComponentInfo*>(info.get()))
            {
                groups[0].push_back(createSlider(sliderInfo));
            }
            else if (auto* toggleInfo = dynamic_cast<ToggleComponentInfo*>(info.get()))
            {
                groups[1].push_back(createToggle(toggleInfo));
            }
            else if (auto* dropdownInfo = dynamic_cast<ComboBoxComponentInfo*>(info.get()))
            {
                groups[2].push_back(createDropdown(dropdownInfo));
            }
            else if (auto* textInfo = dynamic_cast<TextBoxComponentInfo*>(info.get()))
            {
                groups[3].push_back(createTextBox(textInfo));
            }
            else if (auto* numberInfo = dynamic_cast<NumberBoxComponentInfo*>(info.get()))
            {
                groups[4].push_back(createNumberBox(numberInfo));
            }
            else if (auto* multiSelectInfo = dynamic_cast<MultiSelectComponentInfo*>(info.get()))
            {
                groups[5].push_back(createMultiSelect(multiSelectInfo));
            }
            else if (auto* fileChooserInfo = dynamic_cast<FileComponentInfo*>(info.get()))
            {
                groups[6].push_back(createFileChooser(fileChooserInfo));
            }
            else
            {
                // Model::extractInputs creates no other kind of control
                jassertfalse;
            }
        }

        for (auto& group : groups)
        {
            for (auto& control : group)
            {
                addAndMakeVisible(*control);
                controls.push_back(std::move(control));
            }
        }

        resized();
    }

private:
    std::unique_ptr<ControlComponent> createTextBox(TextBoxComponentInfo* info)
    {
        auto textComponent = std::make_unique<TextBoxWithLabel>(info->label);

        auto& textBox = textComponent->getTextBox();

        textComponent->setText(info->value);

        addHandler(&textBox, info);
        textBox.addListener(info);

        return textComponent;
    }

    std::unique_ptr<ControlComponent> createNumberBox(NumberBoxComponentInfo* info)
    {
        auto numberComponent = std::make_unique<NumberBoxWithLabel>(info->label);

        auto& numberBox = numberComponent->getNumberBox();

        numberBox.setRange(info->minimum, info->maximum, info->step);
        numberBox.setValue(info->value, dontSendNotification);

        addHandler(&numberBox, info);
        numberBox.addListener(info);

        return numberComponent;
    }

    std::unique_ptr<ControlComponent> createToggle(ToggleComponentInfo* info)
    {
        auto toggleComponent = std::make_unique<ToggleWithLabel>(info->label);

        auto& toggle = toggleComponent->getToggleButton();

        toggleComponent->setToggleState(info->value, dontSendNotification);

        addHandler(&toggle, info);
        toggle.addListener(info);

        return toggleComponent;
    }

    std::unique_ptr<ControlComponent> createSlider(SliderComponentInfo* info)
    {
        auto sliderComponent =
            std::make_unique<SliderWithLabel>(info->label, Slider::RotaryHorizontalVerticalDrag);

        auto& slider = sliderComponent->getSlider();

        slider.setRange(info->minimum, info->maximum, info->step);
        slider.setValue(info->value);
        slider.setTextBoxStyle(Slider::TextBoxBelow, false, 80, 20);

        addHandler(&slider, info);
        slider.addListener(info);

        return sliderComponent;
    }

    std::unique_ptr<ControlComponent> createDropdown(ComboBoxComponentInfo* info)
    {
        auto dropdownComponent = std::make_unique<ComboBoxWithLabel>(info->label);

        auto& dropdown = dropdownComponent->getComboBox();
        auto font = dropdown.getLookAndFeel().getComboBoxFont(dropdown);
        int widestOptionText = 0;

        for (const auto& option : info->options)
        {
            dropdown.addItem(option, dropdown.getNumItems() + 1);
            widestOptionText = jmax(widestOptionText, ModelStyle::getTextWidth(font, option));
        }

        const auto selected = std::find(info->options.begin(), info->options.end(), info->value);

        if (selected != info->options.end())
        {
            dropdown.setSelectedItemIndex((int) (selected - info->options.begin()),
                                          dontSendNotification);
        }
        else
        {
            // A value outside the choices (e.g., a custom one) is shown as it will be sent
            dropdown.setText(info->value, dontSendNotification);
        }

        dropdown.setTextWhenNoChoicesAvailable("Empty");
        dropdownComponent->setMinimumContentWidth(widestOptionText);

        addHandler(&dropdown, info);
        dropdown.addListener(info);

        return dropdownComponent;
    }

    std::unique_ptr<ControlComponent> createMultiSelect(MultiSelectComponentInfo* info)
    {
        auto multiSelectComponent = std::make_unique<MultiSelectWithLabel>(info->label);

        std::vector<String> options(info->options.begin(), info->options.end());
        std::vector<String> selected(info->values.begin(), info->values.end());

        multiSelectComponent->setOptions(options);
        multiSelectComponent->setSelection(selected);
        multiSelectComponent->addListener(info);

        addHandler(&multiSelectComponent->getSelectionButton(), info);

        return multiSelectComponent;
    }

    std::unique_ptr<ControlComponent> createFileChooser(FileComponentInfo* info)
    {
        auto fileChooserComponent = std::make_unique<FileChooserWithLabel>(info->label);

        if (! info->path.empty())
            fileChooserComponent->setPath(info->path);

        fileChooserComponent->setRequired(info->required);
        fileChooserComponent->setFileTypes(info->fileTypes);

        fileChooserComponent->addListener(info);

        addHandler(fileChooserComponent.get(), info);

        return fileChooserComponent;
    }

    // Shows a control's description in the instructions box while hovering over it
    void addHandler(Component* comp, ModelComponentInfo* info)
    {
        auto handler = std::make_unique<HoverHandler>(*comp);

        handler->onMouseEnter = [this, info]
        {
            if (! info->info.empty())
                instructionsMessage->setMessage(info->info);
        };
        handler->onMouseExit = [this] { instructionsMessage->clearMessage(); };
        handler->attach();

        handlers.push_back(std::move(handler));
    }

    struct RowEntry
    {
        ControlComponent* component = nullptr;

        int width = 0;
        int height = 0;
    };

    /* Fills each row with as many controls as fit, in order, along with the gaps that
       resized() leaves between them and at either end */
    std::vector<std::vector<RowEntry>> buildRowsForWidth(int availableWidth) const
    {
        std::vector<std::vector<RowEntry>> rows;

        if (availableWidth <= 0)
        {
            return rows;
        }

        int rowWidth = 0;

        for (const auto& c : controls)
        {
            const int itemWidth =
                jmin(jmax(c->getMinimumRequiredWidth(), c->getPreferredWidth()), availableWidth);

            if (rows.empty()
                || 2 * minEdgeGap + rowWidth + minInterItemGap + itemWidth > availableWidth)
            {
                rows.emplace_back();
                rowWidth = itemWidth;
            }
            else
            {
                rowWidth += minInterItemGap + itemWidth;
            }

            rows.back().push_back({ c.get(), itemWidth, c->getPreferredHeight() });
        }

        return rows;
    }

    static int getRowHeight(const std::vector<RowEntry>& row)
    {
        int height = 0;

        for (const auto& entry : row)
        {
            height = jmax(height, entry.height);
        }

        return height;
    }

    static constexpr int marginSize = 4;

    static constexpr int minInterItemGap = 6;
    static constexpr int minEdgeGap = 4;
    static constexpr int minRowGap = 6;

    // In the order they are laid out (see updateControls)
    std::vector<std::unique_ptr<ControlComponent>> controls;

    std::vector<std::unique_ptr<HoverHandler>> handlers;

    SharedResourcePointer<InstructionsMessage> instructionsMessage;
};
