/**
 * @file ModelTags.h
 * @brief Model taxonomy and parsing of the tags models declare about themselves.
 * @author cwitkowitz
 *
 * Tags are plain strings, read from a Space's README metadata when browsing models and
 * from the model card once a model is loaded. Structured tags take the form
 * "<key>:<value>" (e.g., "category:separation"), and anything else is a custom tag.
 *
 * The taxonomy is pyharp's (pyharp/pyharp/taxonomy.json), and is where models declare
 * their tags, embedded at build time. A tag it does not recognize is kept as a custom tag,
 * so a model tagged against a newer taxonomy is still shown, just not categorized by it.
 */

#pragma once

#include <algorithm>
#include <iterator>
#include <vector>

#include <TaxonomyData.h>

#include <juce_core/juce_core.h>

using namespace juce;

namespace Taxonomy
{
struct Subcategory
{
    String id;
    String displayName;
};

struct Category
{
    String id;
    String displayName;
    std::vector<Subcategory> subcategories;
};

// In display order, as defined by pyharp's taxonomy.json (embedded when HARP is built)
inline const std::vector<Category>& getCategories()
{
    static const std::vector<Category> categories = []
    {
        std::vector<Category> parsed;

        const var taxonomy = JSON::parse(
            String::fromUTF8(TaxonomyData::taxonomy_json, TaxonomyData::taxonomy_jsonSize));

        if (auto* categoryList = taxonomy["categories"].getArray())
        {
            for (const auto& categoryData : *categoryList)
            {
                Category category { categoryData["id"].toString(),
                                    categoryData["name"].toString(),
                                    {} };

                if (auto* subcategoryList = categoryData["subcategories"].getArray())
                {
                    for (const auto& subcategoryData : *subcategoryList)
                    {
                        category.subcategories.push_back(
                            { subcategoryData["id"].toString(),
                              subcategoryData["name"].toString() });
                    }
                }

                parsed.push_back(std::move(category));
            }
        }

        // Malformed taxonomy data is a build error, which cannot be recovered at run time
        jassert(! parsed.empty());

        return parsed;
    }();

    return categories;
}

inline const Category* findCategory(const String& id)
{
    for (const auto& category : getCategories())
    {
        if (category.id == id)
            return &category;
    }

    return nullptr;
}

// Subcategory ids are unique across the taxonomy, so each one names its category too
inline const Category* findCategoryOfSubcategory(const String& id,
                                                 const Subcategory** subcategory = nullptr)
{
    for (const auto& category : getCategories())
    {
        for (const auto& candidate : category.subcategories)
        {
            if (candidate.id == id)
            {
                if (subcategory != nullptr)
                    *subcategory = &candidate;

                return &category;
            }
        }
    }

    return nullptr;
}
} // namespace Taxonomy

/**
 * What a model's tags say about it.
 */
struct ModelTags
{
    // One kind of data a model takes in or gives back, from tags like "output:file/json"
    struct DataType
    {
        String modality; // e.g. "audio", "midi", "text", "file", "labels"
        StringArray formats; // File extensions it is restricted to, if any, e.g. "json"

        // e.g. "File (JSON, TXT)"
        String describe() const
        {
            String name = modality == "midi"
                              ? String("MIDI")
                              : modality.substring(0, 1).toUpperCase() + modality.substring(1);

            if (! formats.isEmpty())
                name << " (" << formats.joinIntoString(", ").toUpperCase() << ")";

            return name;
        }
    };

    StringArray categories; // Category ids, in taxonomy order
    StringArray subcategories; // Subcategory ids, in taxonomy order
    std::vector<DataType> inputs;
    std::vector<DataType> outputs;
    int sampleRate = 0; // Hz, or 0 if not declared
    String channels; // e.g. "mono", "stereo", "6"
    StringArray custom; // Everything else, as given

    static ModelTags parse(const StringArray& tags)
    {
        ModelTags parsed;

        StringArray categoriesFound;
        StringArray subcategoriesFound;

        for (auto tag : tags)
        {
            tag = tag.trim();

            if (tag.isEmpty())
                continue;

            const String key = tag.upToFirstOccurrenceOf(":", false, false).toLowerCase();
            const String value = tag.fromFirstOccurrenceOf(":", false, false).trim();
            const bool hasValue = tag.containsChar(':') && value.isNotEmpty();

            if (hasValue && key == "category" && Taxonomy::findCategory(value) != nullptr)
            {
                categoriesFound.addIfNotAlreadyThere(value);
            }
            else if (hasValue && key == "subcategory"
                     && Taxonomy::findCategoryOfSubcategory(value) != nullptr)
            {
                subcategoriesFound.addIfNotAlreadyThere(value);
                // A subcategory implies its category
                categoriesFound.addIfNotAlreadyThere(
                    Taxonomy::findCategoryOfSubcategory(value)->id);
            }
            else if (hasValue && (key == "input" || key == "output"))
            {
                addDataType(key == "input" ? parsed.inputs : parsed.outputs, value);
            }
            else if (hasValue && key == "sample-rate" && value.containsOnly("0123456789"))
            {
                parsed.sampleRate = value.getIntValue();
            }
            else if (hasValue && key == "channels")
            {
                parsed.channels = value.toLowerCase();
            }
            else
            {
                parsed.custom.addIfNotAlreadyThere(tag);
            }
        }

        // Keep taxonomy order, regardless of the order in which the tags were declared
        for (const auto& category : Taxonomy::getCategories())
        {
            if (categoriesFound.contains(category.id))
                parsed.categories.add(category.id);

            for (const auto& subcategory : category.subcategories)
            {
                if (subcategoriesFound.contains(subcategory.id))
                    parsed.subcategories.add(subcategory.id);
            }
        }

        return parsed;
    }

    bool isCategorized() const { return ! categories.isEmpty(); }

    bool isInCategory(const String& categoryId) const
    {
        return categories.contains(categoryId);
    }

    // e.g. "Audio, File (JSON, TXT)", or empty when none are declared
    static String describe(const std::vector<DataType>& dataTypes)
    {
        StringArray descriptions;

        for (const auto& dataType : dataTypes)
            descriptions.add(dataType.describe());

        return descriptions.joinIntoString(", ");
    }

    // Short labels for the most distinguishing tags, most important first
    StringArray getDisplayLabels() const
    {
        StringArray labels;

        // The most specific place in the taxonomy: each subcategory, or the category itself
        // where none of its subcategories is given (e.g., "Utility", which has none)
        for (const auto& category : Taxonomy::getCategories())
        {
            if (! categories.contains(category.id))
                continue;

            bool hasSubcategory = false;

            for (const auto& subcategory : category.subcategories)
            {
                if (! subcategories.contains(subcategory.id))
                    continue;

                hasSubcategory = true;

                // "Music" alone is ambiguous, so the analysis subcategories carry their
                // category
                labels.add(category.id == "analysis"
                               ? subcategory.displayName + " " + category.displayName
                               : subcategory.displayName);
            }

            if (! hasSubcategory)
                labels.add(category.displayName);
        }

        if (sampleRate > 0)
            labels.add(sampleRate % 1000 == 0 ? String(sampleRate / 1000) + " kHz"
                                              : String(sampleRate / 1000.0, 1) + " kHz");

        if (channels.isNotEmpty())
            labels.add(channels.containsOnly("0123456789")
                           ? channels + " channels"
                           : channels.substring(0, 1).toUpperCase() + channels.substring(1));

        labels.addArray(custom);

        return labels;
    }

    // Everything a search should match against, lowercased
    String getSearchableText() const
    {
        StringArray words;

        for (const auto& id : categories)
            words.add(Taxonomy::findCategory(id)->displayName);

        words.addArray(getDisplayLabels());
        words.add(describe(inputs));
        words.add(describe(outputs));

        return words.joinIntoString(" ").toLowerCase();
    }

private:
    /* A value is a modality, optionally followed by the formats it is restricted to, e.g.
       "file/json|txt". Tags for the same kind of data (e.g., two file outputs) are merged. */
    static void addDataType(std::vector<DataType>& dataTypes, const String& value)
    {
        const String modality = value.upToFirstOccurrenceOf("/", false, false).trim().toLowerCase();
        const StringArray formats = StringArray::fromTokens(
            value.fromFirstOccurrenceOf("/", false, false).toLowerCase(), "|", "");

        if (modality.isEmpty())
            return;

        auto existing = std::find_if(dataTypes.begin(),
                                     dataTypes.end(),
                                     [&](const DataType& d) { return d.modality == modality; });

        if (existing == dataTypes.end())
        {
            dataTypes.push_back({ modality, {} });
            existing = std::prev(dataTypes.end());
        }

        for (const auto& format : formats)
        {
            if (format.trim().isNotEmpty())
                existing->formats.addIfNotAlreadyThere(format.trim());
        }
    }
};
