#include "juce_gui_basics/juce_gui_basics.h"
#include "TitledTextBox.h"

// Define the wrapper class
class SliderWithLabel : public juce::Component
{
public:
    SliderWithLabel(const juce::String& labelText, juce::Slider::SliderStyle style)
        : slider(style, juce::Slider::TextBoxBelow)
    {
        label.setText(labelText, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
        addAndMakeVisible(slider);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        label.setBounds(bounds.removeFromTop(bounds.getHeight() / 6));
        slider.setBounds(bounds);
        DBG("Slider bounds now considered " + getBounds().toString());
    }

    juce::Slider& getSlider() { return slider; }
    juce::Label& getLabel() { return label; }

private:
    juce::Label label;
    juce::Slider slider;
};


class CtrlComponent: public juce::Component, 
                     public Button::Listener, 
                     public Slider::Listener,
                     public ComboBox::Listener,
                     public TextEditor::Listener
{

public:

  CtrlComponent() {}

  void setModel(std::shared_ptr<WebWave2Wave> model) {
    mModel = model;
  }

  void populateGui() {
    // headerLabel.setText("No model loaded", juce::dontSendNotification);
    // headerLabel.setJustificationType(juce::Justification::centred);
    // addAndMakeVisible(headerLabel);

    if (mModel == nullptr) {
      DBG("populate gui called, but model is null");
      return;
    }

    auto &ctrlList = mModel->controls();
  
    for (const auto &pair : ctrlList) {
      auto ctrlPtr = pair.second;
      // SliderCtrl
        if (auto sliderCtrl = dynamic_cast<SliderCtrl*>(ctrlPtr.get())) {
            auto sliderWithLabel = std::make_unique<SliderWithLabel>(sliderCtrl->label, juce::Slider::RotaryHorizontalVerticalDrag);
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
      } else if (auto toggleCtrl = dynamic_cast<ToggleCtrl*>(ctrlPtr.get())) {
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
      } else if (auto textBoxCtrl = dynamic_cast<TextBoxCtrl*>(ctrlPtr.get())) {
          auto textCtrl = std::make_unique<TitledTextBox>();
          textCtrl->setName(textBoxCtrl->id.toString());
          textCtrl->setTitle(textBoxCtrl->label);
          textCtrl->setText(textBoxCtrl->value);
          textCtrl->addListener(this);
          addAndMakeVisible(*textCtrl);
          textCtrls.push_back(std::move(textCtrl));
          DBG("Text Box: " + textBoxCtrl->label + " added");

      // ComboBoxCtrl
      } else if (auto comboBoxCtrl = dynamic_cast<ComboBoxCtrl*>(ctrlPtr.get())) {
          auto comboBox = std::make_unique<juce::ComboBox>();
          comboBox->setName(comboBoxCtrl->id.toString());
          for (const auto &option : comboBoxCtrl->options) {
              comboBox->addItem(option, comboBox->getNumItems() + 1);
          }

          int selectedId = 1; // Default to first item if the desired value isn't found
          for (int i = 0; i < comboBox->getNumItems(); ++i) {
              if (comboBox->getItemText(i).toStdString() == comboBoxCtrl->value) {
                  selectedId = i + 1;  // item IDs start at 1
                  break;
              }
          }
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

  void resetUI(){
    DBG("CtrlComponent::resetUI called");
    mModel.reset();
    // remove all the widgets and empty the vectors
    for (auto &ctrl : sliders) {
      removeChildComponent(ctrl.get());
    }
    sliders.clear();

    for (auto &ctrl : toggles) {
        removeChildComponent(ctrl.get());
    }
    toggles.clear();

    for (auto &ctrl : optionCtrls) {
        removeChildComponent(ctrl.get());
    }
    optionCtrls.clear();

    for (auto &ctrl : textCtrls) {
        removeChildComponent(ctrl.get());
    }
    textCtrls.clear();
  }


  void resized() override {
      auto area = getLocalBounds();

      // headerLabel.setBounds(area.removeFromTop(30));  // Adjust height to your preference

      juce::FlexBox mainBox;
      mainBox.flexDirection = juce::FlexBox::Direction::column;  // Set the main flex direction to column

      juce::FlexItem::Margin margin(2);

      // Sliders
      juce::FlexBox sliderBox;
      sliderBox.flexDirection = juce::FlexBox::Direction::row;
      for (auto& sliderWithLabel : sliders) {
          DBG("Adding slider with name: " + sliderWithLabel->getSlider().getName() + " to sliderBox");
          sliderBox.items.add(juce::FlexItem(*sliderWithLabel).withFlex(1).withMinWidth(100).withMargin(margin));  // Adjusted min height
      }

      // Toggles
      juce::FlexBox toggleBox;
      toggleBox.flexDirection = juce::FlexBox::Direction::row;
      for (auto& toggle : toggles) {
          DBG("Adding toggle with name: " + toggle->getName() + " to toggleBox");
          toggleBox.items.add(juce::FlexItem(*toggle).withFlex(1).withMinWidth(80).withMargin(margin));
      }

      // Option Controls
      juce::FlexBox optionBox;
      optionBox.flexDirection = juce::FlexBox::Direction::row;
      for (auto& optionCtrl : optionCtrls) {
          DBG("Adding option control with name: " + optionCtrl->getName() + " to optionBox");
          optionBox.items.add(juce::FlexItem(*optionCtrl).withFlex(1).withMinWidth(80).withMargin(margin));
      }

      // Text Controls
      juce::FlexBox textBox;
      textBox.flexDirection = juce::FlexBox::Direction::row;
      for (auto& textCtrl : textCtrls) {
          DBG("Adding text control with name: " + textCtrl->getName() + " to textBox");
          textBox.items.add(juce::FlexItem(*textCtrl).withFlex(0.5).withMinWidth(80).withMargin(margin));
      }

      // Add each FlexBox to the main FlexBox
      if (sliders.size() > 0) {
          mainBox.items.add(juce::FlexItem(sliderBox).withFlex(1).withMinHeight(90));
      }
      if (toggles.size() > 0) {
          mainBox.items.add(juce::FlexItem(toggleBox).withFlex(1).withMinHeight(30));
      }
      if (optionCtrls.size() > 0) {
          mainBox.items.add(juce::FlexItem(optionBox).withFlex(1).withMinHeight(30));
      }
      if (textCtrls.size() > 0) {
          mainBox.items.add(juce::FlexItem(textBox).withFlex(1).withMinHeight(30));
      }

      // Perform Layout
      mainBox.performLayout(area);
  }



  void buttonClicked(Button *button) override {
    auto id = juce::Uuid(button->getName().toStdString());

    CtrlList &ctrls = mModel->controls();
    auto pair = mModel->findCtrlByUuid(id); 
    if (pair == ctrls.end()) {
      DBG("buttonClicked: ctrl not found");
      return;
    }
    auto ctrl = pair->second;
    if (auto toggleCtrl = dynamic_cast<ToggleCtrl*>(ctrl.get())) {
      toggleCtrl->value = button->getToggleState();
    } else {
      DBG("buttonClicked: ctrl is not a toggle");
    }

  }

  void comboBoxChanged(ComboBox *comboBox) override {
    auto id = juce::Uuid(comboBox->getName().toStdString());

    CtrlList &ctrls = mModel->controls();
    auto pair = mModel->findCtrlByUuid(id);
    if (pair == ctrls.end()) {
      DBG("comboBoxChanged: ctrl not found");
      return;
    }
    auto ctrl = pair->second;
    if (auto comboBoxCtrl = dynamic_cast<ComboBoxCtrl*>(ctrl.get())) {
      comboBoxCtrl->value = comboBox->getText().toStdString();
    } else {
      DBG("comboBoxChanged: ctrl is not a combobox");
    }
  }

  void textEditorTextChanged (TextEditor& textEditor) override {
    auto id = juce::Uuid(textEditor.getName().toStdString());

    CtrlList &ctrls = mModel->controls();
    auto pair = mModel->findCtrlByUuid(id);
    if (pair == ctrls.end()) {
      DBG("textEditorTextChanged: ctrl not found");
      return;
    }
    auto ctrl = pair->second;
    if (auto textBoxCtrl = dynamic_cast<TextBoxCtrl*>(ctrl.get())) {
      textBoxCtrl->value = textEditor.getText().toStdString();
    } else {
      DBG("textEditorTextChanged: ctrl is not a text box");
    }
  }

  void sliderValueChanged(Slider* slider) override {
    ignoreUnused(slider);
  }

  void sliderDragEnded(Slider* slider) override {
    auto id = juce::Uuid(slider->getName().toStdString());

    CtrlList &ctrls = mModel->controls();
    auto pair = mModel->findCtrlByUuid(id);
    if (pair == ctrls.end()) {
      DBG("sliderDragEnded: ctrl not found");
      return;
    }
    auto ctrl = pair->second;
    if (auto sliderCtrl = dynamic_cast<SliderCtrl*>(ctrl.get())) {
      sliderCtrl->value = slider->getValue();
    } else if (auto numberBoxCtrl = dynamic_cast<NumberBoxCtrl*>(ctrl.get())) {
      numberBoxCtrl->value = slider->getValue();
    } else {
      DBG("sliderDragEnded: ctrl is not a slider");
    }
  }

private:
  // ToolbarSliderStyle toolbarSliderStyle;
  std::shared_ptr<WebWave2Wave> mModel {nullptr};

  juce::Label headerLabel;
  // HARPLookAndFeel mHARPLookAndFeel;

  // Vectors of unique pointers to widgets
  std::vector<std::unique_ptr<SliderWithLabel>> sliders;  
  std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
  std::vector<std::unique_ptr<juce::ComboBox>> optionCtrls;
  std::vector<std::unique_ptr<TitledTextBox>> textCtrls;

};


