/**
 * @file Labels.h
 * @brief Defines data structures for various types of labels.
 * @author xribene
 */

#pragma once

#include <juce_core/juce_core.h>

using namespace juce;
// RZ: include cloning capabilities in structs to allow copying of polymorphic objects in LabelList without slicing issues.
struct OutputLabel
{
    float t;
    String label;
    std::optional<String> description;
    std::optional<float> duration;
    std::optional<int> color;
    std::optional<String> link;

    virtual ~OutputLabel() = default;

    // ADD THIS TO BASE STRUCT
    virtual std::unique_ptr<OutputLabel> clone() const
    {
        auto copy = std::make_unique<OutputLabel>();
        copyBaseFields(copy.get());
        return copy;
    }

protected:
    void copyBaseFields(OutputLabel* copy) const
    {
        copy->t = t; copy->label = label; copy->description = description;
        copy->duration = duration; copy->color = color; copy->link = link;
    }
};

struct AudioLabel : public OutputLabel
{
    std::optional<float> amplitude;
    // ADD THIS
    std::unique_ptr<OutputLabel> clone() const override {
        auto copy = std::make_unique<AudioLabel>();
        copyBaseFields(copy.get());
        copy->amplitude = amplitude;
        return copy;
    }
};

struct SpectrogramLabel : public OutputLabel
{
    std::optional<float> frequency;
    // ADD THIS
    std::unique_ptr<OutputLabel> clone() const override {
        auto copy = std::make_unique<SpectrogramLabel>();
        copyBaseFields(copy.get());
        copy->frequency = frequency;
        return copy;
    }
};

struct MidiLabel : public OutputLabel
{
    std::optional<float> pitch;
    // ADD THIS
    std::unique_ptr<OutputLabel> clone() const override {
        auto copy = std::make_unique<MidiLabel>();
        copyBaseFields(copy.get());
        copy->pitch = pitch;
        return copy;
    }
};

using LabelList = std::vector<std::unique_ptr<OutputLabel>>;


