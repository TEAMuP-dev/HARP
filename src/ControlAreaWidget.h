#include "WebModel.h"
#include "gui/SliderWithLabel.h"
#include "gui/TitledTextBox.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "utils.h"

class ControlAreaWidget : public juce::Component,
                          public Button::Listener,
                          public Slider::Listener,
                          public ComboBox::Listener,
                          public TextEditor::Listener
{
public:
    ControlAreaWidget() {}

    void setModel(std::shared_ptr<WebModel> model) { mModel = model; }

    void populateControls()
    {
        // headerLabel.setText("No model loaded", juce::dontSendNotification);
        // headerLabel.setJustificationType(juce::Justification::centred);
        // addAndMakeVisible(headerLabel);

        if (mModel == nullptr)
        {
            DBG("populate gui called, but model is null");
            return;
        }

        auto& controlsInfo = mModel->getControlsInfo();
        // // clear the m_ctrls vector
        // m_ctrls.clear();
        // juce::Array<juce::var>& inputComponents = mModel->getControls();

        // // iterate through the list of input components
        // // and choosing only the ones that correspond to input controls and not input media
        for (const auto& pair : controlsInfo)
        {
            auto controlInfo = pair.second;
            // SliderCtrl
            if (auto sliderCtrl = dynamic_cast<SliderInfo*>(controlInfo.get()))
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
            else if (auto toggleCtrl = dynamic_cast<ToggleInfo*>(controlInfo.get()))
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
            else if (auto textBoxCtrl = dynamic_cast<TextBoxInfo*>(controlInfo.get()))
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
            else if (auto comboBoxCtrl = dynamic_cast<ComboBoxInfo*>(controlInfo.get()))
            {
                auto comboBox = std::make_unique<juce::ComboBox>();
                comboBox->setName(comboBoxCtrl->id.toString());
                for (const auto& option : comboBoxCtrl->options)
                {
                    comboBox->addItem(option, comboBox->getNumItems() + 1);
                }

                // int selectedId = 1; // Default to first item if the desired value isn't found
                // for (int i = 0; i < comboBox->getNumItems(); ++i)
                // {
                //     if (comboBox->getItemText(i).toStdString() == comboBoxCtrl->value)
                //     {
                //         selectedId = i + 1; // item IDs start at 1
                //         break;
                //     }
                // }
                comboBox->addListener(this);
                comboBox->setTextWhenNoChoicesAvailable("No choices");
                addAndMakeVisible(*comboBox);
                optionCtrls.push_back(std::move(comboBox));
                DBG("Combo Box: " + comboBoxCtrl->label + " added");
            }
            // TODO: NumberBox (check class HarpNumberBox in pyharp)
        }

        repaint();
        resized();
    }

    void resetUI()
    {
        DBG("ControlAreaWidget::resetUI called");
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

    void resized() override
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
                juce::FlexItem(*toggle).withFlex(1).withMinWidth(80).withMaxHeight(20).withMargin(
                    margin));
        }

        // Option Controls
        juce::FlexBox optionBox;
        optionBox.flexDirection = juce::FlexBox::Direction::row;
        for (auto& optionCtrl : optionCtrls)
        {
            DBG("Adding option control with name: " + optionCtrl->getName() + " to optionBox");
            optionBox.items.add(juce::FlexItem(*optionCtrl)
                                    .withFlex(1)
                                    .withMinWidth(80)
                                    .withMaxHeight(maxHeight)
                                    .withMargin(margin));
        }

        // Text Controls
        juce::FlexBox textBox;
        textBox.flexDirection = juce::FlexBox::Direction::row;
        for (auto& textCtrl : textCtrls)
        {
            DBG("Adding text control with name: " + textCtrl->getName() + " to textBox");
            textBox.items.add(juce::FlexItem(*textCtrl)
                                  .withFlex(1)
                                  .withMinWidth(80)
                                  .withMaxWidth(180)
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

    void buttonClicked(Button* button) override
    {
        auto id = juce::Uuid(button->getName().toStdString());

        // ComponentInfoList& controlsInfo = mModel->getControlsInfo();
        auto componentInfo = mModel->findComponentInfoByUuid(id);
        if (componentInfo == nullptr)
        {
            DBG("buttonClicked: ctrl not found");
            return;
        }
        if (auto toggleInfo = dynamic_cast<ToggleInfo*>(componentInfo.get()))
        {
            toggleInfo->value = button->getToggleState();
        }
        else
        {
            DBG("buttonClicked: ctrl is not a toggle");
        }
    }

    void comboBoxChanged(ComboBox* comboBox) override
    {
        auto id = juce::Uuid(comboBox->getName().toStdString());

        // ComponentInfoList& controlsInfo = mModel->getControlsInfo();
        auto componentInfo = mModel->findComponentInfoByUuid(id);
        if (componentInfo == nullptr)
        {
            DBG("comboBoxChanged: ctrl not found");
            return;
        }
        if (auto comboBoxInfo = dynamic_cast<ComboBoxInfo*>(componentInfo.get()))
        {
            comboBoxInfo->value = comboBox->getText().toStdString();
        }
        else
        {
            DBG("comboBoxChanged: ctrl is not a combobox");
        }
    }

    void textEditorTextChanged(TextEditor& textEditor) override
    {
        auto id = juce::Uuid(textEditor.getName().toStdString());

        // ComponentInfoList& controlsInfo = mModel->getControlsInfo();
        auto componentInfo = mModel->findComponentInfoByUuid(id);
        if (componentInfo == nullptr)
        {
            DBG("textEditorTextChanged: ctrl not found");
            return;
        }
        if (auto textBoxInfo = dynamic_cast<TextBoxInfo*>(componentInfo.get()))
        {
            textBoxInfo->value = textEditor.getText().toStdString();
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

        // ComponentInfoList& controlsInfo = mModel->getControlsInfo();
        auto componentInfo = mModel->findComponentInfoByUuid(id);
        // Check if pair is nullptr
        if (componentInfo == nullptr)
        {
            DBG("sliderDragEnded: ctrl not found");
            return;
        }
        if (auto sliderInfo = dynamic_cast<SliderInfo*>(componentInfo.get()))
        {
            sliderInfo->value = slider->getValue();
        }
        else if (auto numberBoxInfo = dynamic_cast<NumberBoxInfo*>(componentInfo.get()))
        {
            numberBoxInfo->value = slider->getValue();
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

    // Vectors of unique pointers to widgets
    std::vector<std::unique_ptr<SliderWithLabel>> sliders;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::ComboBox>> optionCtrls;
    std::vector<std::unique_ptr<TitledTextBox>> textCtrls;
};
