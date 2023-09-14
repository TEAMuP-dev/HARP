
#include "juce_gui_basics/juce_gui_basics.h"
#include "../UI/DocumentView.h"
#include "../UI/ToolBarStyle.h"
#include "../UI/CustomComponents.h"

#include "EditorView.h"

class CtrlComponent: public juce::Component, 
                     public Button::Listener, 
                     public Slider::Listener,
                     public ComboBox::Listener,
                     public TextEditor::Listener
{
private:
public:

  CtrlComponent() {}

  std::map<std::string, std::any> getParams() {
    // create an empty map
    std::map<std::string, std::any> params;
    for (int i = 0; i < continuousCtrls.size(); i++) {
      params.insert(std::pair<std::string, std::any>(
          continuousCtrls[i]->getName().toStdString(),
          continuousCtrls[i]->getValue()));
    }
    for (int i = 0; i < binaryCtrls.size(); i++) {
      params.insert(std::pair<std::string, std::any>(
          binaryCtrls[i]->getName().toStdString(),
          binaryCtrls[i]->getToggleState()));
    }
    for (int i = 0; i < textCtrls.size(); i++) {
      params.insert(std::pair<std::string, std::any>(
          textCtrls[i]->getName().toStdString(),
          textCtrls[i]->getText().toStdString()));
    }
    for (int i = 0; i < optionCtrls.size(); i++) {
      params.insert(std::pair<std::string, std::any>(
          optionCtrls[i]->getName().toStdString(),
          optionCtrls[i]->getText().toStdString()));
    }
    return params; 
  }

  void populateGui(ListOfDicts& guiAttributes) {

    for (const auto &ctrlAttributes  : guiAttributes) {
        std::string nameId = std::any_cast<std::string>(ctrlAttributes.at("nameId"));
        std::string name = std::any_cast<std::string>(ctrlAttributes.at("name"));
        std::string widgetType = std::any_cast<std::string>(ctrlAttributes.at("widget_type"));
        // int widgetTypeVal = std::any_cast<int>(ctrlAttributes.at("widget_type_val"));
        std::string ctrlType = std::any_cast<std::string>(ctrlAttributes.at("ctrl_type"));

        if (ctrlType == "ContinuousCtrl") {
        // double defaultValue = std::any_cast<double>(ctrlAttributes.at("default"));
        double min = std::any_cast<double>(ctrlAttributes.at("min"));
        double max = std::any_cast<double>(ctrlAttributes.at("max"));
        double step = std::any_cast<double>(ctrlAttributes.at("step"));
        double defautlValue = std::any_cast<double>(ctrlAttributes.at("current"));
        std::unique_ptr<juce::Slider> continuousCtrl = std::make_unique<juce::Slider>();

        if (widgetType == "SLIDER") {
            continuousCtrl->setSliderStyle(juce::Slider::LinearHorizontal);
        }
        else if (widgetType == "ROTARY") {
            continuousCtrl->setSliderStyle(juce::Slider::Rotary);
        }
        else {
            DBG("Unknown widget type" + widgetType + " for ContinuousCtrl");
            jassertfalse;
        }

        continuousCtrl->setName(nameId);
        continuousCtrl->setRange(min, max, step);
        continuousCtrl->setTextBoxStyle(juce::Slider::TextBoxAbove, false, 90, 20);
        continuousCtrl->addListener(this);
        continuousCtrl->setTextValueSuffix(" " + name);
        continuousCtrl->setLookAndFeel(&toolbarSliderStyle);
        continuousCtrl->setValue(defautlValue, juce::dontSendNotification);
        addAndMakeVisible(*continuousCtrl);

        // Add the slider to the vector
        continuousCtrls.push_back(std::move(continuousCtrl));
        }
        
        else if (ctrlType == "BinaryCtrl") {
        bool defaultValue = std::any_cast<bool>(ctrlAttributes.at("current"));
        if (widgetType == "CHECKBOX") {
            std::unique_ptr<juce::ToggleButton> binaryCtrl = std::make_unique<juce::ToggleButton>();
            binaryCtrl->setName(nameId);
            binaryCtrl->setButtonText(name);
            binaryCtrl->setToggleState(defaultValue, juce::dontSendNotification);
            binaryCtrl->addListener(this);
            addAndMakeVisible(*binaryCtrl);
            binaryCtrls.push_back(std::move(binaryCtrl));
        }
        else {
            DBG("Unknown widget type" + widgetType + " for ContinuousCtrl");
            jassertfalse;
        }
        }
        else if (ctrlType == "TextInputCtrl") {
            std::string defaultValue = std::any_cast<std::string>(ctrlAttributes.at("current"));
            std::unique_ptr<TitledTextBox> textCtrl = std::make_unique<TitledTextBox>();
            textCtrl->setName(nameId);
            textCtrl->setTitle(name);
            textCtrl->setText(defaultValue);
            textCtrl->addListener(this);
            addAndMakeVisible(*textCtrl);
            textCtrls.push_back(std::move(textCtrl));
        }
        else if (ctrlType == "FloatInputCtrl") {
            // double defaultValue = std::any_cast<double>(ctrlAttributes.at("default"));
            // TODO
        }
        else if (ctrlType == "OptionCtrl") {
            int defaultValue = std::any_cast<int>(ctrlAttributes.at("current"));
            auto options = std::any_cast<std::vector<std::tuple<std::string, int>>>(ctrlAttributes.at("options"));
            std::unique_ptr<juce::ComboBox> optionCtrl = std::make_unique<juce::ComboBox>();
            for (auto &option : options) {
            optionCtrl->addItem(std::get<0>(option), std::get<1>(option));
            }
            // optionCtrl->setSelectedId(1);
            optionCtrl->setName(nameId);
            optionCtrl->addListener(this);
            optionCtrl->setSelectedId(defaultValue, juce::dontSendNotification);
            optionCtrl->setTextWhenNoChoicesAvailable("No choices");
            addAndMakeVisible(*optionCtrl);
            optionCtrls.push_back(std::move(optionCtrl));
        }
    }
    DBG("populateGui finished");
    
  }


  void resetUI(){
    // remove all the widgets and empty the vectors
    for (auto &ctrl : continuousCtrls) {
      removeChildComponent(ctrl.get());
    }
    continuousCtrls.clear();
    for (auto &ctrl : binaryCtrls) {
      removeChildComponent(ctrl.get());
    }
    binaryCtrls.clear();
    for (auto &ctrl : textCtrls) {
      removeChildComponent(ctrl.get());
    }
    textCtrls.clear();
    for (auto &ctrl : optionCtrls) {
      removeChildComponent(ctrl.get());
    }
    optionCtrls.clear();

    resized();
  }

  void resized() override {
    auto area = getLocalBounds();

    // TODO:: what do these flex numbers do? I feel like I (hugo) could be using them better. 
    juce::FlexBox mainBox;
    {
      mainBox.flexDirection = juce::FlexBox::Direction::row;
      juce::FlexBox ctrlBox1;
      {
          ctrlBox1.flexDirection = juce::FlexBox::Direction::column;
          juce::FlexBox continuousBox;
              // continuousBox.flexWrap = juce::FlexBox::Wrap::noWrap;
              // continuousBox.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
              // continuousBox.alignContent = juce::FlexBox::AlignContent::center;
              continuousBox.flexDirection = juce::FlexBox::Direction::row;
              for (int i = 0; i < continuousCtrls.size(); i++) {
                continuousBox.items.add(juce::FlexItem(*continuousCtrls[i]).withFlex(1));
              }
          ctrlBox1.items.add(juce::FlexItem(continuousBox).withFlex(1));
          juce::FlexBox binaryBox;
              binaryBox.flexDirection = juce::FlexBox::Direction::row;
              for (int i = 0; i < binaryCtrls.size(); i++) {
                binaryBox.items.add(juce::FlexItem(*binaryCtrls[i]).withFlex(1));
              }
          ctrlBox1.items.add(juce::FlexItem(binaryBox).withFlex(1));
      }
      mainBox.items.add(juce::FlexItem(ctrlBox1).withFlex(0.3));

      juce::FlexBox ctrlBox2;
      {
          ctrlBox2.flexDirection = juce::FlexBox::Direction::column;
          juce::FlexBox optionBox;
              optionBox.flexDirection = juce::FlexBox::Direction::row;
              // optionBox.flexWrap = juce::FlexBox::Wrap::noWrap;
      //         optionBox.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;
      //         optionBox.alignContent = juce::FlexBox::AlignContent::center;  
              for (int i = 0; i < optionCtrls.size(); i++) {
                optionBox.items.add(juce::FlexItem(*optionCtrls[i]).withFlex(1));
              }
              if (optionBox.items.size() > 0){
                  ctrlBox2.items.add(juce::FlexItem(optionBox).withFlex(1));
              }
          
          juce::FlexBox textCtrlBox;
              textCtrlBox.flexDirection = juce::FlexBox::Direction::row;
              for (int i = 0; i < textCtrls.size(); i++) {
                textCtrlBox.items.add(juce::FlexItem(*textCtrls[i]).withFlex(1));
              }
              if (textCtrlBox.items.size() > 0){
                  ctrlBox2.items.add(juce::FlexItem(textCtrlBox).withFlex(1));
              }
          // ctrlBox2.items.add(juce::FlexItem(textCtrlBox).withFlex(1));
      }
      mainBox.items.add(juce::FlexItem(ctrlBox2).withFlex(0.3));
      
    }


  }

  void buttonClicked(Button *button) {
    auto name = button->getName().toStdString();
    mEditorView->setCurrentCtrlValue(name, button->getToggleState());
  }

  void comboBoxChanged(ComboBox *comboBox) {
    auto name = comboBox->getName().toStdString();
    mEditorView->setCurrentCtrlValue(name, comboBox->getSelectedId());
  }
  void textEditorReturnKeyPressed (TextEditor& textEditor) {
    auto name = textEditor.getName().toStdString();
    mEditorView->setCurrentCtrlValue(name, textEditor.getText().toStdString());
  }

  void sliderValueChanged(Slider* slider) {
    ignoreUnused(slider);
  }

  void sliderDragEnded(Slider* slider) {
    auto name = slider->getName().toStdString();
    mEditorView->setCurrentCtrlValue(name, slider->getValue());
  }


private:
  void InitGenericDial(
    juce::Slider &dial,
    const juce::String& valueSuffix, 
    const juce::Range<double> range, 
    double step_size,
    float value
  ){
    dial.setLookAndFeel(&toolbarSliderStyle);
    dial.setSliderStyle(Slider::Rotary);
    dial.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 90, 20);
    dial.setTextValueSuffix(valueSuffix);
    dial.setRange(range, step_size);
    dial.setValue(value);
    dial.addListener(this);
  }

private:
  ToolbarSliderStyle toolbarSliderStyle;
  EditorView *mEditorView;

  // Vectors of unique pointers to widgets
  std::vector<std::unique_ptr<juce::Slider>> continuousCtrls;
  std::vector<std::unique_ptr<juce::ToggleButton>> binaryCtrls;
  std::vector<std::unique_ptr<juce::ComboBox>> optionCtrls;
  std::vector<std::unique_ptr<TitledTextBox>> textCtrls;

};


