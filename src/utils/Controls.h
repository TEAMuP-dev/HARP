/**
 * @file Controls.h
 * @brief Defines data structures representing various types of controls, inputs, and outputs.
 * @author xribene
 */

#pragma once

#include <algorithm>

#include <juce_core/juce_core.h>

#include "../gui/FileChooserWithLabel.h"
#include "../gui/MultiSelectWithLabel.h"

#include "Logging.h"

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

/*
  Readers for the fields of a control specification.

  Each returns true when the field was present, held the expected type, and was
  applied. A field that is missing, explicitly null, or of the wrong type leaves
  its destination untouched, so a specification HARP does not understand falls
  back to the control's own defaults rather than to whatever the member happened
  to hold. Returning whether a field was applied also lets a caller tell an
  omitted field from one that was set to the default value.

  A field of the wrong type is logged, since that points at a mismatch between
  HARP and the model rather than at anything the user can correct.
*/

inline void logUnexpectedType(const Identifier& key, const String& expected, const var& value)
{
    DBG_AND_LOG("Controls: Field \"" << key.toString() << "\" was expected to hold a " << expected
                                     << " but holds \"" << value.toString()
                                     << "\". Falling back to the default.");
}

// Retrieves a field that is both present and not null, which is how an unset field arrives
inline bool findField(DynamicObject* input, const Identifier& key, var& property)
{
    if (input == nullptr || ! input->hasProperty(key))
    {
        return false;
    }

    property = input->getProperty(key);

    return ! property.isVoid();
}

inline bool readNumber(DynamicObject* input, const Identifier& key, double& destination)
{
    var property;

    if (! findField(input, key, property))
    {
        return false;
    }

    if (property.isDouble() || property.isInt() || property.isInt64())
    {
        destination = (double) property;

        return true;
    }

    // A provider may quote its numbers, so a string is accepted when it reads as one
    const String text = property.toString().trim();

    if (text.isNotEmpty() && text.containsOnly("0123456789+-.eE"))
    {
        destination = text.getDoubleValue();

        return true;
    }

    logUnexpectedType(key, "number", property);

    return false;
}

inline bool readBool(DynamicObject* input, const Identifier& key, bool& destination)
{
    var property;

    if (! findField(input, key, property))
    {
        return false;
    }

    if (property.isBool() || property.isInt() || property.isInt64() || property.isDouble())
    {
        destination = (bool) property;

        return true;
    }

    if (property.isString())
    {
        destination = stringToBool(property.toString());

        return true;
    }

    logUnexpectedType(key, "boolean", property);

    return false;
}

inline bool readString(DynamicObject* input, const Identifier& key, std::string& destination)
{
    var property;

    if (! findField(input, key, property))
    {
        return false;
    }

    // Anything with structure would stringify into something meaningless to show
    if (property.isArray() || property.isObject() || property.isMethod())
    {
        logUnexpectedType(key, "string", property);

        return false;
    }

    destination = property.toString().toStdString();

    return true;
}

/*
  Appends the entries of a list field.

  Gradio normalizes dropdown choices to (label, value) pairs, so an entry that is
  itself a list contributes only its label, which is both what is displayed and
  what is sent back. A lone value counts as a list of one, which is how a
  multiselect dropdown given a single default arrives.
*/
inline bool readStringList(DynamicObject* input,
                           const Identifier& key,
                           std::vector<std::string>& destination)
{
    var property;

    if (! findField(input, key, property))
    {
        return false;
    }

    const Array<var>* entries = property.getArray();

    if (entries == nullptr)
    {
        if (property.isObject() || property.isMethod())
        {
            logUnexpectedType(key, "list", property);

            return false;
        }

        destination.push_back(property.toString().toStdString());

        return true;
    }

    for (const var& entry : *entries)
    {
        if (const Array<var>* pair = entry.getArray())
        {
            if (! pair->isEmpty())
            {
                destination.push_back(pair->getFirst().toString().toStdString());
            }
        }
        else
        {
            destination.push_back(entry.toString().toStdString());
        }
    }

    return true;
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

        readString(input, "label", label);
        readString(input, "info", info);
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
        readBool(input, "required", required);
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

struct FileComponentInfo : public ModelComponentInfo, public FileChooserWithLabel::Listener
{
    bool required = true;

    std::string path { "" };
    std::vector<std::string> fileTypes;

    FileComponentInfo(DynamicObject* input) : ModelComponentInfo(input)
    {
        readBool(input, "required", required);
        readString(input, "path", path);

        /* An empty list is not an error here: a file chooser with no declared types
           accepts any file, which is what an unrestricted gr.File means. */
        readStringList(input, "file_types", fileTypes);
    }

    void fileChooserChanged(FileChooserWithLabel* fileChooser) override
    {
        path = fileChooser->getPath().toStdString();
    }
};

struct TextBoxComponentInfo : public ModelComponentInfo, public TextEditor::Listener
{
    std::string value { "" };

    TextBoxComponentInfo(DynamicObject* input) : ModelComponentInfo(input)
    {
        readString(input, "value", value);
    }

    void textEditorTextChanged(TextEditor& textEditor) override
    {
        value = textEditor.getText().toStdString();
    }
};

struct NumberBoxComponentInfo : public ModelComponentInfo, public Slider::Listener
{
    /* A number box carries no bounds unless the model sets them, and an unset one
       arrives as null. The box still has to be given a concrete range, so an unset
       bound becomes a wide one. Leaving it at zero would make the range empty and
       pin the box to a single value it could never be edited away from. */
    static constexpr double unboundedLimit = 1.0e9;

    double minimum = -unboundedLimit;
    double maximum = unboundedLimit;

    /* How much the increment / decrement buttons move the value. A zero step
       would make them do nothing, so fall back to whole numbers. */
    double step = 1.0;

    double value = 0.0;

    NumberBoxComponentInfo(DynamicObject* input) : ModelComponentInfo(input)
    {
        readNumber(input, "minimum", minimum);
        readNumber(input, "maximum", maximum);
        readNumber(input, "step", step);

        if (step <= 0.0)
        {
            step = 1.0;
        }

        readNumber(input, "value", value);
    }

    /* Unlike a slider, a number box is edited by typing or by clicking the
       increment buttons, so the value is committed on every change */
    void sliderValueChanged(Slider* slider) override { value = slider->getValue(); }
};

struct ToggleComponentInfo : public ModelComponentInfo, public Button::Listener
{
    bool value = false;

    ToggleComponentInfo(DynamicObject* input) : ModelComponentInfo(input)
    {
        readBool(input, "value", value);
    }

    void buttonClicked(Button* button) override { value = button->getToggleState(); }
};

struct SliderComponentInfo : public ModelComponentInfo, public Slider::Listener
{
    /* The range a gr.Slider falls back on when it declares none of its own */
    double minimum = 0.0;
    double maximum = 100.0;

    /* Zero is the slider's own notion of a continuous range, so unlike the number
       box above it needs no substitute when the model leaves the step unset. */
    double step = 0.0;

    double value = 0.0;

    SliderComponentInfo(DynamicObject* input) : ModelComponentInfo(input)
    {
        readNumber(input, "minimum", minimum);
        readNumber(input, "maximum", maximum);
        readNumber(input, "step", step);

        if (! readNumber(input, "value", value))
        {
            // With no starting value given, the low end of the range is the safe choice
            value = minimum;
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
        readStringList(input, "choices", options);

        if (options.empty())
        {
            /* A dropdown with nothing to choose from cannot be operated. It is left
               empty rather than refused, since the rest of the controls are still
               usable and the model may not depend on this one. */
            DBG_AND_LOG("ComboBoxComponentInfo: Dropdown \"" << String(label)
                                                             << "\" offers no choices.");

            return;
        }

        // Falling back to the first option keeps the box and this value in agreement
        if (! readString(input, "value", value))
        {
            value = options.front();
        }
    }

    void comboBoxChanged(ComboBox* comboBox) override { value = comboBox->getText().toStdString(); }
};

/**
 * A dropdown allowing any number of its options to be selected at once,
 * corresponding to a gr.Dropdown declared with multiselect=True.
 */
struct MultiSelectComponentInfo : public ModelComponentInfo, public MultiSelectWithLabel::Listener
{
    std::vector<std::string> options;

    std::vector<std::string> values;

    MultiSelectComponentInfo(DynamicObject* input) : ModelComponentInfo(input)
    {
        readStringList(input, "choices", options);

        if (options.empty())
        {
            DBG_AND_LOG("MultiSelectComponentInfo: Dropdown \"" << String(label)
                                                                << "\" offers no choices.");
        }

        /* Nothing selected is a valid starting state for a multiselect dropdown, so
           unlike the single-selection box above there is no fallback to apply. */
        readStringList(input, "value", values);
    }

    void multiSelectChanged(MultiSelectWithLabel* multiSelect) override
    {
        values.clear();

        for (const String& selected : multiSelect->getSelection())
        {
            values.push_back(selected.toStdString());
        }
    }
};

using ModelComponentInfoList = std::vector<std::shared_ptr<ModelComponentInfo>>;
