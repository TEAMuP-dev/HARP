/**
 * @file Controls.h
 * @brief Defines data structures representing various types of controls, inputs, and outputs.
 * @author xribene
 */

#pragma once

#include <juce_core/juce_core.h>

using namespace juce;

/**
 * Helper function to convert a boolean string to a C++ boolean
 * value. JUCE doesn't have a built-in function for this purpose.
 */
inline bool stringToBool(const String& str)
{
    String lowerStr = str.toLowerCase();

    if (lowerStr == "true" || lowerStr == "1" || lowerStr == "yes" || lowerStr == "y")
    {
        return true;
    }

    return false;
}

struct ModelComponentInfo
{
    Uuid id { "" };

    std::string label { "" };
    std::string info { "" };

    ModelComponentInfo() = default;
    virtual ~ModelComponentInfo() = default;

    ModelComponentInfo(DynamicObject* input)
    {
        id = Uuid();

        // TODO - check that the following properties are of the correct type

        if (input->hasProperty("label"))
        {
            label = input->getProperty("label").toString().toStdString();
        }
        if (input->hasProperty("info"))
        {
            info = input->getProperty("info").toString().toStdString();
        }
    }
};

struct TrackComponentInfo : public ModelComponentInfo
{
    bool required = true;

    std::string path { "" }; // Used when uploading files

    TrackComponentInfo() = default;
    virtual ~TrackComponentInfo() = default; // Make child-tree polymorphic

    TrackComponentInfo(DynamicObject* input) : ModelComponentInfo(input)
    {
        // TODO - check that the following properties are of the correct type

        if (input->hasProperty("required"))
        {
            required = stringToBool(input->getProperty("required").toString());
        }
    }
};

struct AudioTrackComponentInfo : public TrackComponentInfo
{
    using TrackComponentInfo::TrackComponentInfo;
};

struct MidiTrackComponentInfo : public TrackComponentInfo
{
    using TrackComponentInfo::TrackComponentInfo;
};

struct TextBoxComponentInfo : public ModelComponentInfo, public TextEditor::Listener
{
    std::string value { "" };

    TextBoxComponentInfo(DynamicObject* input) : ModelComponentInfo(input)
    {
        // TODO - check that the following properties are of the correct type

        if (input->hasProperty("value"))
        {
            value = input->getProperty("value").toString().toStdString();
        }
    }

    void textEditorTextChanged(TextEditor& textEditor) override
    {
        value = textEditor.getText().toStdString();
    }
};

struct NumberBoxComponentInfo : public ModelComponentInfo // TODO - Listener
{
    // TODO - consistency with SliderComponent
    double min;
    double max;
    double value;

    NumberBoxComponentInfo(DynamicObject* input) : ModelComponentInfo(input)
    {
        // TODO - check that the following properties are of the correct type

        if (input->hasProperty("min"))
        {
            min = input->getProperty("min").toString().getFloatValue();
        }
        if (input->hasProperty("max"))
        {
            max = input->getProperty("max").toString().getFloatValue();
        }
        if (input->hasProperty("value"))
        {
            value = input->getProperty("value").toString().getFloatValue();
        }
    }
};

struct ToggleComponentInfo : public ModelComponentInfo, public Button::Listener
{
    bool value = false;

    ToggleComponentInfo(DynamicObject* input) : ModelComponentInfo(input)
    {
        // TODO - check that the following properties are of the correct type

        if (input->hasProperty("value"))
        {
            value = stringToBool(input->getProperty("value").toString());
        }
    }

    void buttonClicked(Button* button) override { value = button->getToggleState(); }
};

struct SliderComponentInfo : public ModelComponentInfo, public Slider::Listener
{
    double minimum;
    double maximum;
    double step;
    double value;

    SliderComponentInfo(DynamicObject* input) : ModelComponentInfo(input)
    {
        // TODO - check that the following properties are of the correct type

        if (input->hasProperty("minimum"))
        {
            minimum = input->getProperty("minimum").toString().getFloatValue();
        }
        if (input->hasProperty("maximum"))
        {
            maximum = input->getProperty("maximum").toString().getFloatValue();
        }
        if (input->hasProperty("step"))
        {
            step = input->getProperty("step").toString().getFloatValue();
        }
        if (input->hasProperty("value"))
        {
            value = input->getProperty("value").toString().getFloatValue();
        }
    }

    void sliderValueChanged(Slider* slider) override { ignoreUnused(slider); }
    void sliderDragEnded(Slider* slider) override { value = slider->getValue(); }
};

struct ComboBoxComponentInfo : public ModelComponentInfo, public ComboBox::Listener
{
    std::vector<std::string> options;

    std::string value;

    ComboBoxComponentInfo(DynamicObject* input) : ModelComponentInfo(input)
    {
        // TODO - check that the following properties are of the correct type

        if (input->hasProperty("choices"))
        {
            Array<var>* choices = input->getProperty("choices").getArray();

            if (choices == nullptr)
            {
                // TODO - handle error case: couldn't load choices
            }

            int numChoices = choices->size();

            if (numChoices > 0)
            {
                for (int j = 0; j < numChoices; j++)
                {
                    options.push_back(
                        choices->getReference(j).getArray()->getFirst().toString().toStdString());
                }

                if (! input->hasProperty("value"))
                {
                    // Set selection to first option
                    value = options[0];
                }
                else
                {
                    // Set selection to chosen value
                    value = input->getProperty("value").toString().toStdString();
                }
            }
            else
            {
                // TODO - handle error case: no choices
            }
        }
        else
        {
            // TODO - handle error case: no choises provided
        }
    }

    void comboBoxChanged(ComboBox* comboBox) override { value = comboBox->getText().toStdString(); }
};

using ModelComponentInfoList = std::vector<std::shared_ptr<ModelComponentInfo>>;
