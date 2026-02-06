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
#include "../gui/HoverHandler.h"
#include "../gui/SliderWithLabel.h"
#include "../gui/TextBoxWithLabel.h"

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
        FlexBox controlsArea;
        controlsArea.flexDirection = FlexBox::Direction::row;

        /* Sliders */

        FlexBox slidersArea;
        slidersArea.flexDirection = FlexBox::Direction::row;

        for (auto& slider : sliderComponents)
        {
            slidersArea.items.add(FlexItem(*slider)
                                      .withFlex(1)
                                      .withMinWidth(20)
                                      .withMaxHeight(maxControlHeight)
                                      .withMargin(marginSize));
        }

        if (sliderComponents.size() > 0)
        {
            controlsArea.items.add(
                FlexItem(slidersArea).withFlex(1).withMinHeight(90).withMargin(marginSize));
        }

        /* Toggles */

        FlexBox togglesArea;
        togglesArea.flexDirection = FlexBox::Direction::row;

        for (auto& toggle : toggleComponents)
        {
            togglesArea.items.add(
                FlexItem(*toggle).withFlex(1).withMinWidth(40).withMaxHeight(20).withMargin(
                    marginSize));
        }

        if (toggleComponents.size() > 0)
        {
            controlsArea.items.add(
                FlexItem(togglesArea).withFlex(1).withMinHeight(30).withMargin(marginSize));
        }

        /* Dropdowns */

        FlexBox dropdownsArea;
        dropdownsArea.flexDirection = FlexBox::Direction::row;

        for (auto& dropdown : dropdownComponents)
        {
            dropdownsArea.items.add(
                FlexItem(*dropdown).withFlex(1).withMinWidth(40).withMaxHeight(50).withMargin(
                    marginSize));
        }

        if (dropdownComponents.size() > 0)
        {
            controlsArea.items.add(
                FlexItem(dropdownsArea).withFlex(1).withMinHeight(30).withMargin(marginSize));
        }

        /* Text Boxes */

        FlexBox textBoxArea;
        textBoxArea.flexDirection = FlexBox::Direction::row;

        for (auto& textBox : textComponents)
        {
            textBoxArea.items.add(FlexItem(*textBox)
                                      .withFlex(1)
                                      .withMinWidth(80)
                                      .withMaxWidth(180)
                                      .withMaxHeight(maxControlHeight)
                                      .withMargin(marginSize));
        }

        if (textComponents.size() > 0)
        {
            controlsArea.items.add(
                FlexItem(textBoxArea).withFlex(1).withMinHeight(40).withMargin(marginSize));
        }

        controlsArea.performLayout(getLocalBounds());
    }

    int getNumControls()
    {
        return textComponents.size() + toggleComponents.size() + sliderComponents.size()
               + dropdownComponents.size();
    }

    void resetState()
    {
        for (auto& c : textComponents)
        {
            removeChildComponent(c.get());
        }
        textComponents.clear();

        // TODO - numberComponents

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
            //else if (const auto* numberInfo = dynamic_cast<NumberBoxComponentInfo*>(info.get())) { addNumberBox(numberInfo); }
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

    // TODO - void addNumberBox() {}

    void addToggle(ToggleComponentInfo* info)
    {
        std::unique_ptr<ToggleButton> toggleComponent = std::make_unique<ToggleButton>();

        toggleComponent->setTitle(info->label);
        toggleComponent->setButtonText(info->label);
        toggleComponent->setToggleState(info->value, dontSendNotification);

        addHandler(toggleComponent.get(), info);
        toggleComponent->addListener(info);

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

        for (const auto& option : info->options)
        {
            dropdown.addItem(option, dropdown.getNumItems() + 1);
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

        addHandler(&dropdown, info);
        dropdown.addListener(info);

        addAndMakeVisible(*dropdownComponent);

        dropdownComponents.push_back(std::move(dropdownComponent));
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

    const float marginSize = 4;
    const float maxControlHeight = 100;

    std::vector<std::unique_ptr<TextBoxWithLabel>> textComponents;
    // TODO - numberComponents
    std::vector<std::unique_ptr<ToggleButton>> toggleComponents;
    std::vector<std::unique_ptr<SliderWithLabel>> sliderComponents;
    std::vector<std::unique_ptr<ComboBoxWithLabel>> dropdownComponents;

    std::vector<std::unique_ptr<HoverHandler>> handlers;

    SharedResourcePointer<InstructionsMessage> instructionsMessage;
};
