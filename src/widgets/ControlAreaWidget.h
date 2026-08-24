/**
 * @file ControlAreaWidget.h
 * @brief Component comprising all model controls.
 * @author hugofloresgarcia, xribene, cwitkowitz
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../gui/HoverableLabel.h"
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
    ControlAreaWidget() { resetState(); }
    ~ControlAreaWidget() { resetState(); }

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
            if (row.empty())
            {
                continue;
            }

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

    int getNumControls() const
    {
        return sliderComponents.size() + toggleComponents.size() + dropdownComponents.size()
               + textComponents.size() + numberComponents.size() + multiSelectComponents.size()
               + fileChooserComponents.size();
    }

    int getMinimumRequiredWidth() const
    {
        int requiredWidth = 0;

        auto checkGroup = [&](const auto& group)
        {
            for (const auto& c : group)
            {
                requiredWidth = jmax(requiredWidth, c->getMinimumRequiredWidth());
            }
        };

        checkGroup(sliderComponents);
        checkGroup(toggleComponents);
        checkGroup(dropdownComponents);
        checkGroup(textComponents);
        checkGroup(numberComponents);
        checkGroup(multiSelectComponents);
        checkGroup(fileChooserComponents);

        return requiredWidth + 2 * (marginSize + minEdgeGap);
    }

    int getRequiredHeightForWidth(int width) const
    {
        auto rows = buildRowsForWidth(width - 2 * marginSize);

        if (rows.empty())
        {
            return 0;
        }

        int totalHeight = 2 * marginSize;

        for (size_t i = 0; i < rows.size(); ++i)
        {
            totalHeight += getRowHeight(rows[i]);

            if (i + 1 < rows.size())
            {
                totalHeight += minRowGap;
            }
        }

        return totalHeight;
    }

    void resetState()
    {
        for (auto& c : textComponents)
        {
            removeChildComponent(c.get());
        }
        textComponents.clear();

        for (auto& c : numberComponents)
        {
            removeChildComponent(c.get());
        }
        numberComponents.clear();

        for (auto& c : toggleComponents)
        {
            removeChildComponent(c.get());
        }
        toggleComponents.clear();

        for (auto& c : sliderComponents)
        {
            removeChildComponent(c.get());
        }
        sliderComponents.clear();

        for (auto& c : dropdownComponents)
        {
            removeChildComponent(c.get());
        }
        dropdownComponents.clear();

        for (auto& c : multiSelectComponents)
        {
            removeChildComponent(c.get());
        }
        multiSelectComponents.clear();

        for (auto& c : fileChooserComponents)
        {
            removeChildComponent(c.get());
        }
        fileChooserComponents.clear();

        handlers.clear();
    }

    void updateControls(const ModelComponentInfoList& controlsInfo)
    {
        resetState();

        for (const auto& info : controlsInfo)
        {
            if (auto* textInfo = dynamic_cast<TextBoxComponentInfo*>(info.get()))
            {
                addTextBox(textInfo);
            }
            else if (auto* numberInfo = dynamic_cast<NumberBoxComponentInfo*>(info.get()))
            {
                addNumberBox(numberInfo);
            }
            else if (auto* toggleInfo = dynamic_cast<ToggleComponentInfo*>(info.get()))
            {
                addToggle(toggleInfo);
            }
            else if (auto* sliderInfo = dynamic_cast<SliderComponentInfo*>(info.get()))
            {
                addSlider(sliderInfo);
            }
            else if (auto* dropdownInfo = dynamic_cast<ComboBoxComponentInfo*>(info.get()))
            {
                addDropdown(dropdownInfo);
            }
            else if (auto* multiSelectInfo = dynamic_cast<MultiSelectComponentInfo*>(info.get()))
            {
                addMultiSelect(multiSelectInfo);
            }
            else if (auto* fileChooserInfo = dynamic_cast<FileComponentInfo*>(info.get()))
            {
                addFileChooser(fileChooserInfo);
            }
            else
            {
                // Unsupported control detected
                jassertfalse;
            }
        }

        resized();
    }

private:
    void addTextBox(TextBoxComponentInfo* info)
    {
        std::unique_ptr<TextBoxWithLabel> textComponent =
            std::make_unique<TextBoxWithLabel>(info->label);

        auto& textBox = textComponent->getTextBox();

        textComponent->setText(info->value);

        addHandler(&textBox, info);
        textBox.addListener(info);

        addAndMakeVisible(*textComponent);

        textComponents.push_back(std::move(textComponent));
    }

    void addNumberBox(NumberBoxComponentInfo* info)
    {
        std::unique_ptr<NumberBoxWithLabel> numberComponent =
            std::make_unique<NumberBoxWithLabel>(info->label);

        auto& numberBox = numberComponent->getNumberBox();

        numberBox.setRange(info->minimum, info->maximum, info->step);
        numberBox.setValue(info->value, dontSendNotification);

        addHandler(&numberBox, info);
        numberBox.addListener(info);

        addAndMakeVisible(*numberComponent);

        numberComponents.push_back(std::move(numberComponent));
    }

    void addToggle(ToggleComponentInfo* info)
    {
        std::unique_ptr<ToggleWithLabel> toggleComponent =
            std::make_unique<ToggleWithLabel>(info->label);

        auto& toggle = toggleComponent->getToggleButton();

        toggleComponent->setToggleState(info->value, dontSendNotification);

        addHandler(&toggle, info);
        toggle.addListener(info);

        addAndMakeVisible(*toggleComponent);

        toggleComponents.push_back(std::move(toggleComponent));
    }

    void addSlider(SliderComponentInfo* info)
    {
        std::unique_ptr<SliderWithLabel> sliderComponent =
            std::make_unique<SliderWithLabel>(info->label, Slider::RotaryHorizontalVerticalDrag);

        auto& slider = sliderComponent->getSlider();

        slider.setRange(info->minimum, info->maximum, info->step);
        slider.setValue(info->value);
        slider.setTextBoxStyle(Slider::TextBoxBelow, false, 80, 20);

        addHandler(&slider, info);
        slider.addListener(info);

        addAndMakeVisible(*sliderComponent);

        sliderComponents.push_back(std::move(sliderComponent));
    }

    void addDropdown(ComboBoxComponentInfo* info)
    {
        std::unique_ptr<ComboBoxWithLabel> dropdownComponent =
            std::make_unique<ComboBoxWithLabel>(info->label);

        auto& dropdown = dropdownComponent->getComboBox();
        auto font = dropdown.getLookAndFeel().getComboBoxFont(dropdown);
        int widestOptionText = 0;

        for (const auto& option : info->options)
        {
            dropdown.addItem(option, dropdown.getNumItems() + 1);
            widestOptionText = jmax(widestOptionText, font.getStringWidth(option));
        }

        if (! info->value.empty())
        {
            // Set initial selection if "value" was provided
            dropdown.setSelectedItemIndex(
                std::distance(info->options.begin(),
                              std::find(info->options.begin(), info->options.end(), info->value)),
                dontSendNotification);
        }
        else
        {
            // Fallback to first item
            dropdown.setSelectedItemIndex(0, dontSendNotification);
        }
        dropdown.setTextWhenNoChoicesAvailable("Empty");
        dropdownComponent->setMinimumContentWidth(widestOptionText);

        addHandler(&dropdown, info);
        dropdown.addListener(info);

        addAndMakeVisible(*dropdownComponent);

        dropdownComponents.push_back(std::move(dropdownComponent));
    }

    void addMultiSelect(MultiSelectComponentInfo* info)
    {
        std::unique_ptr<MultiSelectWithLabel> multiSelectComponent =
            std::make_unique<MultiSelectWithLabel>(info->label);

        std::vector<String> options;

        for (const auto& option : info->options)
        {
            options.push_back(String(option));
        }

        multiSelectComponent->setOptions(options);

        multiSelectComponent->isOptionSelected = [info](const String& option)
        { return info->isSelected(option.toStdString()); };

        multiSelectComponent->onOptionToggled = [info](const String& option, bool isSelected)
        { info->setSelected(option.toStdString(), isSelected); };

        multiSelectComponent->updateSelectionText();

        addHandler(&multiSelectComponent->getSelectionButton(), info);

        addAndMakeVisible(*multiSelectComponent);

        multiSelectComponents.push_back(std::move(multiSelectComponent));
    }

    void addFileChooser(FileComponentInfo* info)
    {
        std::unique_ptr<FileChooserWithLabel> fileChooserComponent =
            std::make_unique<FileChooserWithLabel>(info->label);

        if (! info->path.empty())
            fileChooserComponent->setPath(info->path);

        fileChooserComponent->setRequired(info->required);
        fileChooserComponent->setFileTypes(info->fileTypes);

        fileChooserComponent->onFileSelected = [info](const String& path)
        { info->path = path.toStdString(); };

        addHandler(fileChooserComponent.get(), info);

        addAndMakeVisible(*fileChooserComponent);

        fileChooserComponents.push_back(std::move(fileChooserComponent));
    }

    void addHandler(Component* comp, ModelComponentInfo* info)
    {
        std::unique_ptr<HoverHandler> handler = std::make_unique<HoverHandler>(*comp);

        handler->onMouseEnter = [this, info]() { setInstructions(info->info); };
        handler->onMouseExit = [this]() { clearInstructions(); };
        handler->attach();

        handlers.push_back(std::move(handler));
    }

    void setInstructions(const String& text)
    {
        if (text.isNotEmpty() && instructionsMessage != nullptr)
        {
            instructionsMessage->setMessage(text);
        }
    }

    void clearInstructions()
    {
        if (instructionsMessage != nullptr)
        {
            instructionsMessage->clearMessage();
        }
    }

    struct RowEntry
    {
        ControlComponent* component = nullptr;

        int width = 0;
        int height = 0;
    };

    std::vector<std::vector<RowEntry>> buildRowsForWidth(int width) const
    {
        std::vector<std::vector<RowEntry>> rows;

        if (width <= 0)
        {
            return rows;
        }

        addGroupToRows(rows, sliderComponents, width);
        addGroupToRows(rows, toggleComponents, width);
        addGroupToRows(rows, dropdownComponents, width);
        addGroupToRows(rows, textComponents, width);
        addGroupToRows(rows, numberComponents, width);
        addGroupToRows(rows, multiSelectComponents, width);
        addGroupToRows(rows, fileChooserComponents, width);

        return rows;
    }

    void addGroupToRows(std::vector<std::vector<RowEntry>>& rows,
                        const auto& components,
                        int availableWidth) const
    {
        for (const auto& c : components)
        {
            int itemWidth = jmax(c->getMinimumRequiredWidth(), c->getPreferredWidth());

            itemWidth = jmin(itemWidth, availableWidth);

            if (rows.empty())
            {
                rows.emplace_back();
            }

            auto& row = rows.back();

            int currentWidth = 0;

            for (const auto& entry : row)
            {
                currentWidth += entry.width;
            }

            if (! row.empty())
            {
                currentWidth += minInterItemGap * (int) row.size();
            }

            int candidateWidth = currentWidth + (row.empty() ? 0 : minInterItemGap) + itemWidth;

            if (! row.empty() && candidateWidth > availableWidth)
            {
                rows.emplace_back();
            }

            rows.back().push_back({ c.get(), itemWidth, c->getPreferredHeight() });
        }
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

    static constexpr float marginSize = 4;


    static constexpr int minInterItemGap = 6;
    static constexpr int minEdgeGap = 4;
    static constexpr int minRowGap = 6;

    std::vector<std::unique_ptr<TextBoxWithLabel>> textComponents;
    std::vector<std::unique_ptr<NumberBoxWithLabel>> numberComponents;
    std::vector<std::unique_ptr<ToggleWithLabel>> toggleComponents;
    std::vector<std::unique_ptr<SliderWithLabel>> sliderComponents;
    std::vector<std::unique_ptr<ComboBoxWithLabel>> dropdownComponents;
    std::vector<std::unique_ptr<MultiSelectWithLabel>> multiSelectComponents;
    std::vector<std::unique_ptr<FileChooserWithLabel>> fileChooserComponents;

    std::vector<std::unique_ptr<HoverHandler>> handlers;

    SharedResourcePointer<InstructionsMessage> instructionsMessage;
};
