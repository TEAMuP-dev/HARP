#include "WebModel.h"
#include "gui/SliderWithLabel.h"
#include "gui/TitledTextBox.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "utils.h"

class CtrlComponent : public juce::Component,
                      public Button::Listener,
                      public Slider::Listener,
                      public ComboBox::Listener,
                      public TextEditor::Listener
{
public:
    CtrlComponent() {}

    void setModel(std::shared_ptr<WebModel> model) { mModel = model; }

    void populateGui()
    {
        // headerLabel.setText("No model loaded", juce::dontSendNotification);
        // headerLabel.setJustificationType(juce::Justification::centred);
        // addAndMakeVisible(headerLabel);

        if (mModel == nullptr)
        {
            DBG("populate gui called, but model is null");
            return;
        }

        auto& ctrlList = mModel->controls();

        for (const auto& pair : ctrlList)
        {
            auto ctrlPtr = pair.second;
            // SliderCtrl
            if (auto sliderCtrl = dynamic_cast<SliderCtrl*>(ctrlPtr.get()))
            {
                auto sliderWithLabel = std::make_unique<SliderWithLabel>(
                    sliderCtrl->label, juce::Slider::RotaryHorizontalVerticalDrag);
                // auto& label = sliderWithLabel->getLabel();
                // label.setColour(juce::Label::ColourIds::textColourId, mHARPLookAndFeel.textHeaderColor);
                auto& slider = sliderWithLabel->getSlider();
                slider.setName(sliderCtrl->id.toString());
                slider.setRange(sliderCtrl->minimum, sliderCtrl->maximum, sliderCtrl->step);
                slider.setValue(sliderCtrl->value);
                slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
                slider.addListener(this);
                addAndMakeVisible(*sliderWithLabel);
                sliders.push_back(std::move(sliderWithLabel));
                DBG("Slider: " + sliderCtrl->label + " added");

                // ToggleCtrl
            }
            else if (auto toggleCtrl = dynamic_cast<ToggleCtrl*>(ctrlPtr.get()))
            {
                auto toggle = std::make_unique<juce::ToggleButton>();
                toggle->setName(toggleCtrl->id.toString());
                toggle->setTitle(toggleCtrl->label);
                toggle->setButtonText(toggleCtrl->label);
                toggle->setToggleState(toggleCtrl->value, juce::dontSendNotification);
                toggle->addListener(this);
                addAndMakeVisible(*toggle);
                toggles.push_back(std::move(toggle));
                DBG("Toggle: " + toggleCtrl->label + " added");

                // TextBoxCtrl
            }
            else if (auto textBoxCtrl = dynamic_cast<TextBoxCtrl*>(ctrlPtr.get()))
            {
                auto textCtrl = std::make_unique<TitledTextBox>();
                textCtrl->setName(textBoxCtrl->id.toString());
                textCtrl->setTitle(textBoxCtrl->label);
                textCtrl->setText(textBoxCtrl->value);
                textCtrl->addListener(this);
                addAndMakeVisible(*textCtrl);
                textCtrls.push_back(std::move(textCtrl));
                DBG("Text Box: " + textBoxCtrl->label + " added");

                // ComboBoxCtrl
            }
            else if (auto comboBoxCtrl = dynamic_cast<ComboBoxCtrl*>(ctrlPtr.get()))
            {
                auto comboBox = std::make_unique<juce::ComboBox>();
                comboBox->setName(comboBoxCtrl->id.toString());
                for (const auto& option : comboBoxCtrl->options)
                {
                    comboBox->addItem(option, comboBox->getNumItems() + 1);
                }

                int selectedId = 1; // Default to first item if the desired value isn't found
                for (int i = 0; i < comboBox->getNumItems(); ++i)
                {
                    if (comboBox->getItemText(i).toStdString() == comboBoxCtrl->value)
                    {
                        selectedId = i + 1; // item IDs start at 1
                        break;
                    }
                }
                comboBox->setSelectedId(selectedId, juce::dontSendNotification);
                comboBox->addListener(this);
                comboBox->setTextWhenNoChoicesAvailable("No choices");
                addAndMakeVisible(*comboBox);
                optionCtrls.push_back(std::move(comboBox));
                DBG("Combo Box: " + comboBoxCtrl->label + " added");
            }
        }
        repaint();
        resized();
    }

    void resetUI()
    {
        DBG("CtrlComponent::resetUI called");
        mModel.reset();
        // remove all the widgets and empty the vectors
        for (auto& ctrl : sliders)
        {
            removeChildComponent(ctrl.get());
        }
        sliders.clear();

        for (auto& ctrl : toggles)
        {
            removeChildComponent(ctrl.get());
        }
        toggles.clear();

        for (auto& ctrl : optionCtrls)
        {
            removeChildComponent(ctrl.get());
        }
        optionCtrls.clear();

        for (auto& ctrl : textCtrls)
        {
            removeChildComponent(ctrl.get());
        }
        textCtrls.clear();
    }

    void resized2() 
    {
        auto area = getLocalBounds();

        // headerLabel.setBounds(area.removeFromTop(30));  // Adjust height to your preference

        juce::FlexBox mainBox;
        mainBox.flexDirection =
            juce::FlexBox::Direction::column; // Set the main flex direction to column

        juce::FlexItem::Margin margin(2);

        // Sliders
        juce::FlexBox sliderBox;
        sliderBox.flexDirection = juce::FlexBox::Direction::row;
        for (auto& sliderWithLabel : sliders)
        {
            DBG("Adding slider with name: " + sliderWithLabel->getSlider().getName()
                + " to sliderBox");
            sliderBox.items.add(juce::FlexItem(*sliderWithLabel)
                                    .withFlex(1)
                                    .withMinWidth(100)
                                    .withMargin(margin)); // Adjusted min height
        }

        // Toggles
        juce::FlexBox toggleBox;
        toggleBox.flexDirection = juce::FlexBox::Direction::row;
        for (auto& toggle : toggles)
        {
            DBG("Adding toggle with name: " + toggle->getName() + " to toggleBox");
            toggleBox.items.add(
                juce::FlexItem(*toggle).withFlex(1).withMinWidth(80).withMargin(margin));
        }

        // Option Controls
        juce::FlexBox optionBox;
        optionBox.flexDirection = juce::FlexBox::Direction::row;
        for (auto& optionCtrl : optionCtrls)
        {
            DBG("Adding option control with name: " + optionCtrl->getName() + " to optionBox");
            optionBox.items.add(
                juce::FlexItem(*optionCtrl).withFlex(1).withMinWidth(80).withMargin(margin));
        }

        // Text Controls
        juce::FlexBox textBox;
        textBox.flexDirection = juce::FlexBox::Direction::row;
        for (auto& textCtrl : textCtrls)
        {
            DBG("Adding text control with name: " + textCtrl->getName() + " to textBox");
            textBox.items.add(
                juce::FlexItem(*textCtrl).withFlex(0.5).withMinWidth(80).withMargin(margin));
        }

        // Add each FlexBox to the main FlexBox
        if (sliders.size() > 0)
        {
            mainBox.items.add(juce::FlexItem(sliderBox).withFlex(1).withMinHeight(90));
        }
        if (toggles.size() > 0)
        {
            mainBox.items.add(juce::FlexItem(toggleBox).withFlex(1).withMinHeight(30));
        }
        if (optionCtrls.size() > 0)
        {
            mainBox.items.add(juce::FlexItem(optionBox).withFlex(1).withMinHeight(30));
        }
        if (textCtrls.size() > 0)
        {
            mainBox.items.add(juce::FlexItem(textBox).withFlex(1).withMinHeight(30));
        }

        // Perform Layout
        mainBox.performLayout(area);
    }

    void resized1() 
    {
        /*
        place sliders side by side, with minWidth 30 and max 50
        place toggles first in their own column with minWidth 30 and maxWidth 50, and then put them in the same row as sliders.
        place textboxes side by side with minWIdth 80 and max 200
        Precalculate the length of all the controls side by side and dynamically set the number of rows. 
        For example if 1 slider 1 toggle and 1 textbox, their total min width is 30 + 30 + 80 = 140
        so if the area.width() is 200, then we can fit all of them in one row.
        If however, the area.width() is 100, then we need to put them in two rows.
        Another example, if 2 sliders, 2 toggles and 2 textboxes, their total min width is 60 + 30 + 160 = 250
        The reason that I don't count the width of toggles 2 times is because they are first placed in their own column.
        
        */
        auto area = getLocalBounds();

        // headerLabel.setBounds(area.removeFromTop(30));  // Adjust height to your preference

        juce::FlexBox mainBox;
        mainBox.flexDirection =
            juce::FlexBox::Direction::row; // Set the main flex direction to column


        // juce::FlexItem::Margin margin(2);
        int margin = 4;
        int maxHeight = 100;
        // Sliders
        juce::FlexBox sliderBox;
        sliderBox.flexDirection = juce::FlexBox::Direction::row;
        for (auto& sliderWithLabel : sliders)
        {
            DBG("Adding slider with name: " + sliderWithLabel->getSlider().getName()
                + " to sliderBox");
            sliderBox.items.add(juce::FlexItem(*sliderWithLabel)
                                    .withFlex(1)
                                    .withMinWidth(20)
                                    .withMaxHeight(maxHeight)
                                    .withMargin(margin)); // Adjusted min height
        }

        // Toggles
        juce::FlexBox toggleBox;
        toggleBox.flexDirection = juce::FlexBox::Direction::row;
        for (auto& toggle : toggles)
        {
            DBG("Adding toggle with name: " + toggle->getName() + " to toggleBox");
            toggleBox.items.add(
                juce::FlexItem(*toggle).withFlex(1)
                                        .withMinWidth(80)
                                        .withMaxHeight(20)
                                        .withMargin(margin));
        }

        // Option Controls
        juce::FlexBox optionBox;
        optionBox.flexDirection = juce::FlexBox::Direction::row;
        for (auto& optionCtrl : optionCtrls)
        {
            DBG("Adding option control with name: " + optionCtrl->getName() + " to optionBox");
            optionBox.items.add(
                juce::FlexItem(*optionCtrl).withFlex(1)
                                            .withMinWidth(80)
                                            .withMaxHeight(40)
                                            // .withMaxHeight(maxHeight)
                                            .withMargin(margin));
        }

        // Text Controls
        juce::FlexBox textBox;
        textBox.flexDirection = juce::FlexBox::Direction::row;
        for (auto& textCtrl : textCtrls)
        {
            DBG("Adding text control with name: " + textCtrl->getName() + " to textBox");
            textBox.items.add(
                juce::FlexItem(*textCtrl).withFlex(1).withMinWidth(80).withMaxWidth(180)
                    .withMaxHeight(maxHeight)
                    .withMargin(margin));
        }

        // Add each FlexBox to the main FlexBox
        if (sliders.size() > 0)
        {
            mainBox.items.add(juce::FlexItem(sliderBox).withFlex(1).withMinHeight(90));
        }
        if (toggles.size() > 0)
        {
            mainBox.items.add(
                juce::FlexItem(toggleBox).withFlex(1).withMinHeight(30).withMargin(margin));
        }
        if (optionCtrls.size() > 0)
        {
            mainBox.items.add(
                juce::FlexItem(optionBox).withFlex(1).withMinHeight(30).withMargin(margin));
        }
        if (textCtrls.size() > 0)
        {
            mainBox.items.add(
                juce::FlexItem(textBox).withFlex(1).withMinHeight(40).withMargin(margin));
        }

        // Perform Layout
        mainBox.performLayout(area);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        int availableWidth = area.getWidth();
        int margin = 4;

        juce::FlexBox mainBox;
        mainBox.flexDirection =
            juce::FlexBox::Direction::row; // Set the main flex direction to column

        // --------------------
        // Toggles: Vertical Column
        // --------------------
        juce::FlexBox toggleColumn;
        toggleColumn.flexDirection = juce::FlexBox::Direction::column;
        for (auto& toggle : toggles)
        {
            DBG("Adding toggle with name: " + toggle->getName() + " to toggleColumn");
            toggleColumn.items.add(
                juce::FlexItem(*toggle)
                    .withFlex(1)
                    .withMinWidth(30)
                    .withMaxWidth(150)
                    .withMaxHeight(40)
                    .withMargin(margin));
        }

        // The toggle column as a FlexItem
        toggleColumn.justifyContent = juce::FlexBox::JustifyContent::center;
        juce::FlexItem toggleColumnItem(toggleColumn);
        // toggleColumnItem.withMinWidth(30)
        //                 .withMaxWidth(50)
        //                 .withMargin(margin);

        // --------------------
        // Sliders: Horizontal Row
        // --------------------
        juce::FlexBox sliderRow;
        sliderRow.flexDirection = juce::FlexBox::Direction::row;
        // sliderRow.flexWrap = juce::FlexBox::Wrap::wrap; // Allow wrapping to next line
        for (auto& sliderWithLabel : sliders)
        {
            DBG("Adding slider with name: " + sliderWithLabel->getSlider().getName()
                + " to sliderRow");
            sliderRow.items.add(
                juce::FlexItem(*sliderWithLabel)
                    .withFlex(1)
                    .withMinWidth(40)
                    .withMaxWidth(100)
                    .withMinHeight(80)
                    .withMargin(margin));
        }
        sliderRow.justifyContent = juce::FlexBox::JustifyContent::center;
        // The slider row as a FlexItem
        juce::FlexItem sliderRowItem(sliderRow);
        // sliderRowItem.withMinWidth(30)
        //              .withMaxWidth(100)
        //              .withMargin(margin);

        // --------------------
        // Text Controls: Horizontal Row
        // --------------------
        juce::FlexBox textBoxRow;
        textBoxRow.flexDirection = juce::FlexBox::Direction::row;
        // textBoxRow.flexWrap = juce::FlexBox::Wrap::wrap;
        for (auto& textCtrl : textCtrls)
        {
            DBG("Adding text control with name: " + textCtrl->getName() + " to textBoxRow");
            textBoxRow.items.add(
                juce::FlexItem(*textCtrl)
                    .withFlex(1)
                    // .withMinWidth(200)
                    .withMaxWidth(250)
                    // .withMaxHeight(100)
                    .withMargin(margin));
        }

        // The text box row as a FlexItem
        textBoxRow.justifyContent = juce::FlexBox::JustifyContent::center;
        juce::FlexItem textBoxRowItem(textBoxRow);
        // textBoxRowItem.withMinWidth(200)
                        // .withMaxWidth(250)
        //                 .withMaxHeight(100)
        //                 .withMargin(margin);

        // --------------------
        // Option Controls: Horizontal Row
        // --------------------
        juce::FlexBox optionColumn;
        optionColumn.flexDirection = juce::FlexBox::Direction::column;
        // optionColumn.flexWrap = juce::FlexBox::Wrap::wrap;
        for (auto& optionCtrl : optionCtrls)
        {
            DBG("Adding option control with name: " + optionCtrl->getName() + " to optionRow");
            optionColumn.items.add(
                juce::FlexItem(*optionCtrl)
                    .withFlex(1)
                    .withMinWidth(80)
                    .withMaxWidth(200)
                    .withMaxHeight(40)
                    .withMargin(margin));
        }
        // The option column as a FlexItem
        optionColumn.justifyContent = juce::FlexBox::JustifyContent::center;
        juce::FlexItem optionColumnItem(optionColumn);
        optionColumnItem.withMinWidth(80)
                        .withMaxWidth(200)
                        .withMargin(margin);


        // --------------------
        // Arrange Controls in Rows
        // --------------------
        // juce::FlexBox mainBox;
        // mainBox.flexDirection = juce::FlexBox::Direction::column;

        // First Row: Toggles and Sliders
        // juce::FlexBox firstRow;
        // firstRow.flexDirection = juce::FlexBox::Direction::row;
        // firstRow.flexWrap = juce::FlexBox::Wrap::wrap;

        // int firstRowMinWidth = 0;

        // if (!toggles.empty())
        // {
        //     firstRow.items.add(toggleColumnItem);
        //     firstRowMinWidth += static_cast<int>(toggleColumnItem.minWidth + toggleColumnItem.margin.left + toggleColumnItem.margin.right);
        // }

        // // Calculate how many sliders can fit in the first row after the toggle column
        // int availableWidthForSliders = availableWidth - firstRowMinWidth;
        // int currentRowWidth = 0;

        // juce::Array<juce::FlexItem> sliderItems = sliderRow.items;
        // juce::FlexBox slidersInFirstRow;
        // slidersInFirstRow.flexDirection = juce::FlexBox::Direction::row;

        // for (auto& sliderItem : sliderItems)
        // {
        //     int itemWidth = static_cast<int>(sliderItem.minWidth + sliderItem.margin.left + sliderItem.margin.right);

        //     if (currentRowWidth + itemWidth > availableWidthForSliders && currentRowWidth > 0)
        //     {
        //         // Can't fit more sliders in the first row
        //         break;
        //     }

        //     slidersInFirstRow.items.add(sliderItem);
        //     currentRowWidth += itemWidth;
        // }

        // Add Sliders Row
        if (!sliders.empty())
        {
            mainBox.items.add(sliderRowItem.withFlex(int((sliders.size() + 1) / 2)));
        }

        // Add Option Controls Row
        if (!optionCtrls.empty())
        {
            mainBox.items.add(optionColumnItem.withFlex(1));
        }

        // Add Toggles Column
        if (!toggles.empty())
        {
            mainBox.items.add(toggleColumnItem.withFlex(1));
        }

        // Add Text Controls Row
        if (!textCtrls.empty())
        {
            mainBox.items.add(textBoxRowItem.withFlex(2 * textCtrls.size()));
        }

        // Perform the layout
        mainBox.performLayout(area);
    }


    void buttonClicked(Button* button) override
    {
        auto id = juce::Uuid(button->getName().toStdString());

        CtrlList& ctrls = mModel->controls();
        auto pair = mModel->findCtrlByUuid(id);
        if (pair == ctrls.end())
        {
            DBG("buttonClicked: ctrl not found");
            return;
        }
        auto ctrl = pair->second;
        if (auto toggleCtrl = dynamic_cast<ToggleCtrl*>(ctrl.get()))
        {
            toggleCtrl->value = button->getToggleState();
        }
        else
        {
            DBG("buttonClicked: ctrl is not a toggle");
        }
    }

    void comboBoxChanged(ComboBox* comboBox) override
    {
        auto id = juce::Uuid(comboBox->getName().toStdString());

        CtrlList& ctrls = mModel->controls();
        auto pair = mModel->findCtrlByUuid(id);
        if (pair == ctrls.end())
        {
            DBG("comboBoxChanged: ctrl not found");
            return;
        }
        auto ctrl = pair->second;
        if (auto comboBoxCtrl = dynamic_cast<ComboBoxCtrl*>(ctrl.get()))
        {
            comboBoxCtrl->value = comboBox->getText().toStdString();
        }
        else
        {
            DBG("comboBoxChanged: ctrl is not a combobox");
        }
    }

    void textEditorTextChanged(TextEditor& textEditor) override
    {
        auto id = juce::Uuid(textEditor.getName().toStdString());

        CtrlList& ctrls = mModel->controls();
        auto pair = mModel->findCtrlByUuid(id);
        if (pair == ctrls.end())
        {
            DBG("textEditorTextChanged: ctrl not found");
            return;
        }
        auto ctrl = pair->second;
        if (auto textBoxCtrl = dynamic_cast<TextBoxCtrl*>(ctrl.get()))
        {
            textBoxCtrl->value = textEditor.getText().toStdString();
        }
        else
        {
            DBG("textEditorTextChanged: ctrl is not a text box");
        }
    }

    void sliderValueChanged(Slider* slider) override { ignoreUnused(slider); }

    void sliderDragEnded(Slider* slider) override
    {
        auto id = juce::Uuid(slider->getName().toStdString());

        CtrlList& ctrls = mModel->controls();
        auto pair = mModel->findCtrlByUuid(id);
        if (pair == ctrls.end())
        {
            DBG("sliderDragEnded: ctrl not found");
            return;
        }
        auto ctrl = pair->second;
        if (auto sliderCtrl = dynamic_cast<SliderCtrl*>(ctrl.get()))
        {
            sliderCtrl->value = slider->getValue();
        }
        else if (auto numberBoxCtrl = dynamic_cast<NumberBoxCtrl*>(ctrl.get()))
        {
            numberBoxCtrl->value = slider->getValue();
        }
        else
        {
            DBG("sliderDragEnded: ctrl is not a slider");
        }
    }

private:
    // ToolbarSliderStyle toolbarSliderStyle;
    std::shared_ptr<WebModel> mModel { nullptr };

    juce::Label headerLabel;
    // HARPLookAndFeel mHARPLookAndFeel;

    // Vectors of unique pointers to widgets
    std::vector<std::unique_ptr<SliderWithLabel>> sliders;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::ComboBox>> optionCtrls;
    std::vector<std::unique_ptr<TitledTextBox>> textCtrls;
};
