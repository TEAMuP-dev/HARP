// -----------------------------------------------------------------------------
// Test rationale
// -----------------------------------------------------------------------------
// This file verifies deterministic, HARP-owned parsing and state-update behavior
// in Controls.h. These tests focus on the internal control metadata objects that
// HARP builds from DynamicObject data before the GUI renders or updates controls.
//
// The helper functions create compact DynamicObject and choice structures so
// tests can focus on parsing behavior instead of repeated JUCE setup.
//
// The ControlsGuiTest fixture initializes JUCE GUI state for listener tests that
// exercise TextEditor, ToggleButton, Slider, and ComboBox callbacks.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../../src/utils/Controls.h"

using namespace juce;

namespace
{
// Creates an empty DynamicObject for default/missing-property parsing tests.
DynamicObject::Ptr makeObject()
{
    return new DynamicObject();
}

// Creates a DynamicObject with named properties to keep parser tests compact.
DynamicObject::Ptr makeObject(std::initializer_list<std::pair<const char*, var>> properties)
{
    DynamicObject::Ptr object = new DynamicObject();

    for (const auto& [key, value] : properties)
    {
        object->setProperty(Identifier(key), value);
    }

    return object;
}

// Creates one Gradio-style combo-box choice, represented as an array whose first
// element is the displayed value parsed by ComboBoxComponentInfo.
var makeChoice(const var& displayedValue)
{
    Array<var> choice;
    choice.add(displayedValue);
    return var(choice);
}

// Creates the choices array consumed by ComboBoxComponentInfo.
var makeChoices(std::initializer_list<var> choices)
{
    Array<var> choiceArray;

    for (const auto& choice : choices)
    {
        choiceArray.add(choice);
    }

    return var(choiceArray);
}

// Initializes JUCE GUI infrastructure required by listener/callback tests.
class ControlsGuiTest : public ::testing::Test
{
protected:
    ScopedJuceInitialiser_GUI juceInitialiser;
};
} // namespace

// -----------------------------------------------------------------------------
// Category: Boolean parsing
// -----------------------------------------------------------------------------
// These tests verify stringToBool behavior used by multiple control parsers.

// Verifies that stringToBool accepts every true representation used by HARP parsers.
TEST(StringToBoolTest, AcceptsDocumentedTrueValues)
{
    const StringArray trueValues { "true", "TRUE", "True", "tRuE", "1", "yes", "YES", "Yes", "y", "Y" };

    for (const auto& input : trueValues)
    {
        EXPECT_TRUE(stringToBool(input)) << "Expected true for input: " << input;
    }
}


// Verifies that boolean parsing is whitespace-sensitive and does not trim input.
TEST(StringToBoolTest, DoesNotTrimWhitespace)
{
    const StringArray whitespaceInputs { "", " ", "    ", " true", "true ", " yes ", "\ntrue", "true\n", "\ttrue" };

    for (const auto& input : whitespaceInputs)
    {
        EXPECT_FALSE(stringToBool(input)) << "Expected false for whitespace-sensitive input: " << input;
    }
}

// Verifies that unsupported boolean-like strings fail closed to false.
TEST(StringToBoolTest, ReturnsFalseForUnsupportedValues)
{
    const StringArray unsupportedValues { "banana", "10", "-1", "null", "nullptr", "maybe", "truthy" };

    for (const auto& input : unsupportedValues)
    {
        EXPECT_FALSE(stringToBool(input)) << "Expected false for unsupported input: " << input;
    }
}

// -----------------------------------------------------------------------------
// Category: Base control metadata parsing
// -----------------------------------------------------------------------------
// These tests verify shared label/info parsing in ModelComponentInfo.

// Verifies that the base metadata object starts with empty display text.
TEST(ModelComponentInfoTest, DefaultConstructorInitializesTextFields)
{
    ModelComponentInfo info;

    EXPECT_EQ(info.label, "");
    EXPECT_EQ(info.info, "");
}

// Verifies that missing label/info properties preserve base metadata defaults.
TEST(ModelComponentInfoTest, EmptyDynamicObjectUsesDefaultTextFields)
{
    auto object = makeObject();
    ModelComponentInfo info(object.get());

    EXPECT_EQ(info.label, "");
    EXPECT_EQ(info.info, "");
}

// Verifies that shared label/info metadata is parsed from DynamicObject input.
TEST(ModelComponentInfoTest, ParsesLabelAndInfo)
{
    auto object = makeObject({ { "label", "Pitch" }, { "info", "Controls pitch shifting" } });
    ModelComponentInfo info(object.get());

    EXPECT_EQ(info.label, "Pitch");
    EXPECT_EQ(info.info, "Controls pitch shifting");
}

// Verifies that label parsing does not require an info property.
TEST(ModelComponentInfoTest, ParsesOnlyLabelWhenInfoIsMissing)
{
    auto object = makeObject({ { "label", "Gain" } });
    ModelComponentInfo info(object.get());

    EXPECT_EQ(info.label, "Gain");
    EXPECT_EQ(info.info, "");
}

// Verifies that info parsing does not require a label property.
TEST(ModelComponentInfoTest, ParsesOnlyInfoWhenLabelIsMissing)
{
    auto object = makeObject({ { "info", "Useful helper text" } });
    ModelComponentInfo info(object.get());

    EXPECT_EQ(info.label, "");
    EXPECT_EQ(info.info, "Useful helper text");
}

// Verifies that non-string label/info properties are converted using JUCE var string conversion.
TEST(ModelComponentInfoTest, ConvertsNonStringPropertiesToStrings)
{
    auto object = makeObject({ { "label", 123 }, { "info", 45.5 } });
    ModelComponentInfo info(object.get());

    EXPECT_EQ(info.label, "123");
    EXPECT_EQ(info.info, "45.5");
}

// Verifies that metadata parsing preserves Unicode and long strings.
TEST(ModelComponentInfoTest, PreservesUnicodeAndLongStrings)
{
    const String unicodeLabel = CharPointer_UTF8("音量 🎵");
    const String longInfo = String::repeatedString("abc", 1000);

    auto object = makeObject({ { "label", unicodeLabel }, { "info", longInfo } });
    ModelComponentInfo info(object.get());

    EXPECT_EQ(info.label, unicodeLabel.toStdString());
    EXPECT_EQ(info.info, longInfo.toStdString());
}

// -----------------------------------------------------------------------------
// Category: Track metadata parsing
// -----------------------------------------------------------------------------
// These tests verify required-track defaults and derived track parsing.

// Verifies default track metadata, including required=true.
TEST(TrackComponentInfoTest, DefaultConstructorRequiredIsTrue)
{
    TrackComponentInfo info;

    EXPECT_TRUE(info.required);
    EXPECT_EQ(info.path, "");
}

// Verifies that missing required metadata defaults to a required track.
TEST(TrackComponentInfoTest, MissingRequiredPropertyDefaultsToTrue)
{
    auto object = makeObject({ { "label", "Input Track" } });
    TrackComponentInfo info(object.get());

    EXPECT_TRUE(info.required);
}

// Verifies that required=true values use the same boolean parsing contract as stringToBool.
TEST(TrackComponentInfoTest, ParsesRequiredTrueValues)
{
    const StringArray trueValues { "true", "TRUE", "1", "yes", "y" };

    for (const auto& value : trueValues)
    {
        auto object = makeObject({ { "required", value } });
        TrackComponentInfo info(object.get());
        EXPECT_TRUE(info.required) << "Expected required=true for input: " << value;
    }
}

// Verifies that false and unsupported required values are parsed as false.
TEST(TrackComponentInfoTest, ParsesRequiredFalseAndUnsupportedValuesAsFalse)
{
    const StringArray falseValues { "false", "FALSE", "0", "no", "n", "banana", " true " };

    for (const auto& value : falseValues)
    {
        auto object = makeObject({ { "required", value } });
        TrackComponentInfo info(object.get());
        EXPECT_FALSE(info.required) << "Expected required=false for input: " << value;
    }
}

// Verifies that audio and MIDI track metadata reuse TrackComponentInfo parsing behavior.
TEST(TrackComponentInfoTest, AudioAndMidiTrackComponentsUseTrackParsing)
{
    auto object = makeObject({ { "label", "Track" }, { "required", "false" } });

    AudioTrackComponentInfo audioInfo(object.get());
    MidiTrackComponentInfo midiInfo(object.get());

    EXPECT_EQ(audioInfo.label, "Track");
    EXPECT_FALSE(audioInfo.required);
    EXPECT_EQ(midiInfo.label, "Track");
    EXPECT_FALSE(midiInfo.required);
}

// -----------------------------------------------------------------------------
// Category: Text metadata and listener behavior
// -----------------------------------------------------------------------------
// These tests verify TextBoxComponentInfo parsing and TextEditor callback updates.

// Verifies that missing text-box values default to an empty string.
TEST(TextBoxComponentInfoTest, MissingValueDefaultsToEmptyString)
{
    auto object = makeObject({ { "label", "Prompt" } });
    TextBoxComponentInfo info(object.get());

    EXPECT_EQ(info.value, "");
}

// Verifies that text-box values are parsed from DynamicObject input.
TEST(TextBoxComponentInfoTest, ParsesValue)
{
    auto object = makeObject({ { "value", "hello" } });
    TextBoxComponentInfo info(object.get());

    EXPECT_EQ(info.value, "hello");
}

// Verifies that non-string text-box values are converted to strings.
TEST(TextBoxComponentInfoTest, ConvertsNonStringValueToString)
{
    auto object = makeObject({ { "value", 42 } });
    TextBoxComponentInfo info(object.get());

    EXPECT_EQ(info.value, "42");
}

// Verifies that text-editor callbacks update the stored text-box value.
TEST_F(ControlsGuiTest, TextEditorListenerUpdatesStoredValue)
{
    auto object = makeObject({ { "value", "initial" } });
    TextBoxComponentInfo info(object.get());

    TextEditor editor;
    editor.setText("updated", dontSendNotification);
    info.textEditorTextChanged(editor);

    EXPECT_EQ(info.value, "updated");
}

// Verifies that text-editor callbacks preserve Unicode user input.
TEST_F(ControlsGuiTest, TextEditorListenerPreservesUnicodeText)
{
    const String unicodeText = CharPointer_UTF8("こんにちは 🎵");
    auto object = makeObject({ { "value", "initial" } });
    TextBoxComponentInfo info(object.get());

    TextEditor editor;
    editor.setText(unicodeText, dontSendNotification);
    info.textEditorTextChanged(editor);

    EXPECT_EQ(info.value, unicodeText.toStdString());
}

// -----------------------------------------------------------------------------
// Category: Numeric metadata parsing
// -----------------------------------------------------------------------------
// These tests verify NumberBoxComponentInfo numeric parsing behavior.

// Verifies numeric field parsing for explicit numeric values.
TEST(NumberBoxComponentInfoTest, ParsesAllNumericFields)
{
    auto object = makeObject({ { "min", -1.5 }, { "max", 10.25 }, { "value", 3.75 } });
    NumberBoxComponentInfo info(object.get());

    EXPECT_DOUBLE_EQ(info.min, -1.5);
    EXPECT_DOUBLE_EQ(info.max, 10.25);
    EXPECT_DOUBLE_EQ(info.value, 3.75);
}

// Verifies numeric field parsing from string-backed values.
TEST(NumberBoxComponentInfoTest, ParsesNumericStringFields)
{
    auto object = makeObject({ { "min", "-10" }, { "max", "20.5" }, { "value", "7.25" } });
    NumberBoxComponentInfo info(object.get());

    EXPECT_DOUBLE_EQ(info.min, -10.0);
    EXPECT_DOUBLE_EQ(info.max, 20.5);
    EXPECT_DOUBLE_EQ(info.value, 7.25);
}

// Verifies current malformed numeric-string behavior so parser assumptions remain explicit.
TEST(NumberBoxComponentInfoTest, MalformedNumericStringsParseAsZero)
{
    auto object = makeObject({ { "min", "abc" }, { "max", "" }, { "value", "not-a-number" } });
    NumberBoxComponentInfo info(object.get());

    EXPECT_DOUBLE_EQ(info.min, 0.0);
    EXPECT_DOUBLE_EQ(info.max, 0.0);
    EXPECT_DOUBLE_EQ(info.value, 0.0);
}

// -----------------------------------------------------------------------------
// Category: Toggle metadata and listener behavior
// -----------------------------------------------------------------------------
// These tests verify ToggleComponentInfo parsing and ToggleButton callback updates.

// Verifies that missing toggle values default to false.
TEST(ToggleComponentInfoTest, MissingValueDefaultsToFalse)
{
    auto object = makeObject({ { "label", "Enable" } });
    ToggleComponentInfo info(object.get());

    EXPECT_FALSE(info.value);
}

// Verifies toggle value parsing for supported true values.
TEST(ToggleComponentInfoTest, ParsesTrueValues)
{
    const StringArray trueValues { "true", "TRUE", "1", "yes", "y" };

    for (const auto& value : trueValues)
    {
        auto object = makeObject({ { "value", value } });
        ToggleComponentInfo info(object.get());
        EXPECT_TRUE(info.value) << "Expected toggle value=true for input: " << value;
    }
}

// Verifies toggle value parsing for false and unsupported values.
TEST(ToggleComponentInfoTest, ParsesFalseAndUnsupportedValuesAsFalse)
{
    const StringArray falseValues { "false", "FALSE", "0", "no", "n", "banana", " true " };

    for (const auto& value : falseValues)
    {
        auto object = makeObject({ { "value", value } });
        ToggleComponentInfo info(object.get());
        EXPECT_FALSE(info.value) << "Expected toggle value=false for input: " << value;
    }
}

// Verifies that button callbacks update the stored toggle value in both directions.
TEST_F(ControlsGuiTest, ButtonListenerUpdatesStoredToggleValue)
{
    auto object = makeObject({ { "value", "false" } });
    ToggleComponentInfo info(object.get());

    ToggleButton button;
    button.setToggleState(true, dontSendNotification);
    info.buttonClicked(&button);
    EXPECT_TRUE(info.value);

    button.setToggleState(false, dontSendNotification);
    info.buttonClicked(&button);
    EXPECT_FALSE(info.value);
}

// -----------------------------------------------------------------------------
// Category: Slider metadata and listener behavior
// -----------------------------------------------------------------------------
// These tests verify SliderComponentInfo parsing and Slider callback update behavior.

// Verifies slider field parsing for explicit numeric values.
TEST(SliderComponentInfoTest, ParsesAllSliderFields)
{
    auto object = makeObject({ { "minimum", -5.0 }, { "maximum", 5.0 }, { "step", 0.5 }, { "value", 2.5 } });
    SliderComponentInfo info(object.get());

    EXPECT_DOUBLE_EQ(info.minimum, -5.0);
    EXPECT_DOUBLE_EQ(info.maximum, 5.0);
    EXPECT_DOUBLE_EQ(info.step, 0.5);
    EXPECT_DOUBLE_EQ(info.value, 2.5);
}

// Verifies that SliderComponentInfo preserves invalid ranges instead of validating them.
TEST(SliderComponentInfoTest, PreservesMinGreaterThanMaxWithoutValidation)
{
    auto object = makeObject({ { "minimum", 10.0 }, { "maximum", -10.0 }, { "step", 1.0 }, { "value", 0.0 } });
    SliderComponentInfo info(object.get());

    EXPECT_DOUBLE_EQ(info.minimum, 10.0);
    EXPECT_DOUBLE_EQ(info.maximum, -10.0);
    EXPECT_DOUBLE_EQ(info.step, 1.0);
    EXPECT_DOUBLE_EQ(info.value, 0.0);
}

// Verifies slider field parsing from string-backed numeric values.
TEST(SliderComponentInfoTest, ParsesNumericStrings)
{
    auto object = makeObject({ { "minimum", "-1" }, { "maximum", "1.5" }, { "step", "0.25" }, { "value", "0.75" } });
    SliderComponentInfo info(object.get());

    EXPECT_DOUBLE_EQ(info.minimum, -1.0);
    EXPECT_DOUBLE_EQ(info.maximum, 1.5);
    EXPECT_DOUBLE_EQ(info.step, 0.25);
    EXPECT_DOUBLE_EQ(info.value, 0.75);
}

// Verifies that transient slider movement does not update stored value.
TEST_F(ControlsGuiTest, SliderValueChangedDoesNotUpdateStoredValue)
{
    auto object = makeObject({ { "minimum", 0.0 }, { "maximum", 10.0 }, { "step", 1.0 }, { "value", 2.0 } });
    SliderComponentInfo info(object.get());

    Slider slider;
    slider.setValue(8.0, dontSendNotification);
    info.sliderValueChanged(&slider);

    EXPECT_DOUBLE_EQ(info.value, 2.0);
}

// Verifies that drag-end slider callbacks commit the stored value.
TEST_F(ControlsGuiTest, SliderDragEndedUpdatesStoredValue)
{
    auto object = makeObject({ { "minimum", 0.0 }, { "maximum", 10.0 }, { "step", 1.0 }, { "value", 2.0 } });
    SliderComponentInfo info(object.get());

    Slider slider;
    slider.setValue(8.0, dontSendNotification);
    info.sliderDragEnded(&slider);

    EXPECT_DOUBLE_EQ(info.value, 8.0);
}

// -----------------------------------------------------------------------------
// Category: Combo box metadata and listener behavior
// -----------------------------------------------------------------------------
// These tests verify ComboBoxComponentInfo choice parsing and ComboBox updates.

// Verifies that missing combo-box choices produce empty options and value.
TEST(ComboBoxComponentInfoTest, MissingChoicesDefaultsToEmptyOptionsAndValue)
{
    auto object = makeObject({ { "label", "Mode" } });
    ComboBoxComponentInfo info(object.get());

    EXPECT_TRUE(info.options.empty());
    EXPECT_EQ(info.value, "");
}

// Verifies that an explicit empty choices array produces empty options and value.
TEST(ComboBoxComponentInfoTest, EmptyChoicesDefaultsToEmptyOptionsAndValue)
{
    Array<var> choices;
    auto object = makeObject({ { "choices", var(choices) } });
    ComboBoxComponentInfo info(object.get());

    EXPECT_TRUE(info.options.empty());
    EXPECT_EQ(info.value, "");
}

// Verifies that missing combo-box value defaults to the first available choice.
TEST(ComboBoxComponentInfoTest, MissingValueDefaultsToFirstChoice)
{
    auto object = makeObject({ { "choices", makeChoices({ makeChoice("A"), makeChoice("B") }) } });
    ComboBoxComponentInfo info(object.get());

    ASSERT_EQ(info.options.size(), 2u);
    EXPECT_EQ(info.options[0], "A");
    EXPECT_EQ(info.options[1], "B");
    EXPECT_EQ(info.value, "A");
}

// Verifies that an explicit combo-box value overrides first-choice defaulting.
TEST(ComboBoxComponentInfoTest, ProvidedValueOverridesDefaultSelection)
{
    auto object = makeObject({ { "choices", makeChoices({ makeChoice("A"), makeChoice("B") }) }, { "value", "B" } });
    ComboBoxComponentInfo info(object.get());

    ASSERT_EQ(info.options.size(), 2u);
    EXPECT_EQ(info.value, "B");
}

// Verifies that explicit combo-box values are preserved even when absent from options.
TEST(ComboBoxComponentInfoTest, ProvidedValueDoesNotNeedToMatchOption)
{
    auto object = makeObject({ { "choices", makeChoices({ makeChoice("A"), makeChoice("B") }) }, { "value", "C" } });
    ComboBoxComponentInfo info(object.get());

    ASSERT_EQ(info.options.size(), 2u);
    EXPECT_EQ(info.value, "C");
}

// Verifies that duplicate and empty combo-box choices are preserved.
TEST(ComboBoxComponentInfoTest, PreservesDuplicateAndEmptyChoices)
{
    auto object = makeObject({ { "choices", makeChoices({ makeChoice("A"), makeChoice("A"), makeChoice("") }) } });
    ComboBoxComponentInfo info(object.get());

    ASSERT_EQ(info.options.size(), 3u);
    EXPECT_EQ(info.options[0], "A");
    EXPECT_EQ(info.options[1], "A");
    EXPECT_EQ(info.options[2], "");
    EXPECT_EQ(info.value, "A");
}

// Verifies that non-string combo-box choice labels are converted to strings.
TEST(ComboBoxComponentInfoTest, ConvertsChoiceLabelsToStrings)
{
    auto object = makeObject({ { "choices", makeChoices({ makeChoice(123), makeChoice(45.5) }) } });
    ComboBoxComponentInfo info(object.get());

    ASSERT_EQ(info.options.size(), 2u);
    EXPECT_EQ(info.options[0], "123");
    EXPECT_EQ(info.options[1], "45.5");
    EXPECT_EQ(info.value, "123");
}

// Verifies that combo-box parsing preserves Unicode choices.
TEST(ComboBoxComponentInfoTest, PreservesUnicodeChoices)
{
    const String first = CharPointer_UTF8("音色");
    const String second = CharPointer_UTF8("リズム 🎵");
    auto object = makeObject({ { "choices", makeChoices({ makeChoice(first), makeChoice(second) }) } });
    ComboBoxComponentInfo info(object.get());

    ASSERT_EQ(info.options.size(), 2u);
    EXPECT_EQ(info.options[0], first.toStdString());
    EXPECT_EQ(info.options[1], second.toStdString());
    EXPECT_EQ(info.value, first.toStdString());
}

// Verifies that combo-box callbacks update the stored selection.
TEST_F(ControlsGuiTest, ComboBoxListenerUpdatesStoredSelection)
{
    auto object = makeObject({ { "choices", makeChoices({ makeChoice("A"), makeChoice("B") }) } });
    ComboBoxComponentInfo info(object.get());

    ComboBox comboBox;
    comboBox.addItem("A", 1);
    comboBox.addItem("B", 2);
    comboBox.setSelectedId(2, dontSendNotification);
    info.comboBoxChanged(&comboBox);

    EXPECT_EQ(info.value, "B");
}
