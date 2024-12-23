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

        // // clear the m_ctrls vector
        // m_ctrls.clear();
        juce::Array<juce::var>& inputComponents = mModel->getInputComponents();

        // // iterate through the list of input components
        // // and choosing only the ones that correspond to input controls and not input media
        for (int i = 0; i < inputComponents.size(); i++)
        {
            juce::var ctrl = inputComponents.getReference(i);
            // Remove this if. it's already been checked in webmodel.h loading
            // if (! ctrl.isObject())
            // {
            //     status2 = ModelStatus::ERROR;
            //     error.devMessage = "Failed to load controls from JSON. ctrl is not an object.";
            //     return OpResult::fail(error);
            // }

            try
            {
                // get the ctrl type
                juce::String ctrl_type = ctrl["type"].toString().toStdString();

                // For the first two, we are abusing the term control.
                // They are actually the main inputs to the model (audio or midi)
                // if (ctrl_type == "audio_in")
                // {
                //     // CB:TODO: NOT USED ANYWHERE
                //     // ControlAreaWidget.h ignores this when populating the GUI
                //     auto audio_in = std::make_shared<AudioInCtrl>();
                //     audio_in->label = ctrl["label"].toString().toStdString();

                //     m_ctrls.push_back({ audio_in->id, audio_in });
                //     LogAndDBG("Audio In: " + audio_in->label + " added");
                // }
                // else if (ctrl_type == "midi_in")
                // {
                //     auto midi_in = std::make_shared<MidiInCtrl>();
                //     midi_in->label = ctrl["label"].toString().toStdString();

                //     m_ctrls.push_back({ midi_in->id, midi_in });
                //     LogAndDBG("MIDI In: " + midi_in->label + " added");
                // }
                // The rest are the actual controls that map to hyperparameters
                // of the model
                if (ctrl_type == "slider")
                {
                    // auto slider = std::make_shared<SliderCtrl>();
                    auto id = juce::Uuid();
                    auto label = ctrl["label"].toString().toStdString();
                    auto minimum = ctrl["minimum"].toString().getFloatValue();
                    auto maximum = ctrl["maximum"].toString().getFloatValue();
                    auto step = ctrl["step"].toString().getFloatValue();
                    auto value = ctrl["value"].toString().getFloatValue();

                    // m_ctrls.push_back({ slider->id, slider });
                    // LogAndDBG("Slider: " + slider->label + " added");

                    auto sliderWithLabel = std::make_unique<SliderWithLabel>(
                        label, juce::Slider::RotaryHorizontalVerticalDrag);
                    // auto& label = sliderWithLabel->getLabel();
                    // label.setColour(juce::Label::ColourIds::textColourId, mHARPLookAndFeel.textHeaderColor);
                    auto& slider = sliderWithLabel->getSlider();
                    slider.setName(id.toString());
                    slider.setRange(minimum, maximum, step);
                    slider.setValue(value);
                    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
                    slider.addListener(this);
                    addAndMakeVisible(*sliderWithLabel);
                    sliders.push_back(std::move(sliderWithLabel));
                    DBG("Slider: " + label + " added");
                }
                else if (ctrl_type == "text")
                {
                    // auto text = std::make_shared<TextBoxCtrl>();
                    auto id = juce::Uuid();
                    auto label = ctrl["label"].toString().toStdString();
                    auto value = ctrl["value"].toString().toStdString();

                    // m_ctrls.push_back({ text->id, text });
                    // LogAndDBG("Text: " + text->label + " added");


                    auto textCtrl = std::make_unique<TitledTextBox>();
                    textCtrl->setName(id.toString());
                    textCtrl->setTitle(label);
                    textCtrl->setText(value);
                    textCtrl->addListener(this);
                    addAndMakeVisible(*textCtrl);
                    textCtrls.push_back(std::move(textCtrl));
                    DBG("Text Box: " + label + " added");

                }
                else if (ctrl_type == "number_box")
                {
                    // auto number_box = std::make_shared<NumberBoxCtrl>();
                    auto id = juce::Uuid();
                    auto label = ctrl["label"].toString().toStdString();
                    auto min = ctrl["min"].toString().getFloatValue();
                    auto max = ctrl["max"].toString().getFloatValue();
                    auto value = ctrl["value"].toString().getFloatValue();

                    // m_ctrls.push_back({ number_box->id, number_box });
                    LogAndDBG("Number Box: " + label + " added");
                    // TODO : Implement a number_box component

                }
                else if (ctrl_type == "toggle")
                {
                    auto id = juce::Uuid();
                    auto label = ctrl["label"].toString().toStdString();
                    auto value = 1; //ctrl["value"].toString().toStdString();

                    LogAndDBG("Toggle not implemented yet");
                    auto toggle = std::make_unique<juce::ToggleButton>();
                    toggle->setName(id.toString());
                    toggle->setTitle(label);
                    toggle->setButtonText(label);
                    toggle->setToggleState(value, juce::dontSendNotification);
                    toggle->addListener(this);
                    addAndMakeVisible(*toggle);
                    toggles.push_back(std::move(toggle));
                    DBG("Toggle: " + label + " added");
                }
                else if (ctrl_type == "dropdown")
                {
                    auto id = juce::Uuid();
                    auto label = ctrl["label"].toString().toStdString();
                    auto value = ctrl["value"].toString().toStdString();
                    // auto options = ctrl["choices"].toString().toStdString();

                    auto comboBox = std::make_unique<juce::ComboBox>();
                    comboBox->setName(id.toString());
                    // for (const auto& option : options)
                    // {
                    //     comboBox->addItem(option, comboBox->getNumItems() + 1);
                    // }

                    int selectedId = 1; // Default to first item if the desired value isn't found
                    for (int j = 0; j < comboBox->getNumItems(); ++j)
                    {
                        if (comboBox->getItemText(j).toStdString() == value)
                        {
                            selectedId = j + 1; // item IDs start at 1
                            break;
                        }
                    }
                    comboBox->addListener(this);
                    comboBox->setTextWhenNoChoicesAvailable("No choices");
                    addAndMakeVisible(*comboBox);
                    optionCtrls.push_back(std::move(comboBox));
                    DBG("Combo Box: " + label + " added");
                }
                else
                    LogAndDBG("failed to parse control with unknown type: " + ctrl_type);
            }
            catch (const char* e)
            {
                // status2 = ModelStatus::ERROR;
                // error.devMessage = "Failed to load controls from JSON. " + std::string(e);
                // return OpResult::fail(error);
                throw "it shouldn't throw here";
            }
        }

        // OLD CODE
        // }
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
