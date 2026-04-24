/**
 * @file ModelRegistry.h
 * @brief Accessors for the bundled model registry.
 */

#pragma once

#include <vector>

#include <BinaryData.h>
#include <juce_core/juce_core.h>

#include "Logging.h"

using namespace juce;

namespace ModelRegistry
{
inline std::vector<std::string> getFallbackFeaturedModelPaths()
{
    return {
        "click here to enter a custom path...",
        "stability/text-to-audio",
        "stability/audio-to-audio",
        "teamup-tech/text2midi-symbolic-music-generation",
        "teamup-tech/demucs-source-separation",
        "teamup-tech/solo-piano-audio-to-midi-transcription",
        "teamup-tech/transkun", // TODO - more intuitive name
        "teamup-tech/TRIA", // TODO - more intuitive name: (The Rhythm In Anything) conditional drum generation
        "teamup-tech/anticipatory-music-transformer",
        "teamup-tech/vampnet-conditional-music-generation",
        "teamup-tech/harmonic-percussive-separation",
        "teamup-tech/Kokoro-TTS",
        "teamup-tech/MegaTTS3-Voice-Cloning",
        "teamup-tech/midi-synthesizer",
        "teamup-tech/audioseal", // TODO - more intuitive name
        // "xribene/HARP-UI-TEST-v3"
    };
}

inline std::vector<std::string> getFeaturedModelPaths()
{
    const String registryJson =
        String::fromUTF8(BinaryData::model_registry_json, BinaryData::model_registry_jsonSize);

    var parsedRegistry;
    const Result parseResult = JSON::parse(registryJson, parsedRegistry);

    if (parseResult.failed() || ! parsedRegistry.isObject())
    {
        DBG_AND_LOG("ModelRegistry::getFeaturedModelPaths: Failed to parse bundled registry. "
                    "Using fallback list.");

        return getFallbackFeaturedModelPaths();
    }

    auto* root = parsedRegistry.getDynamicObject();

    if (root == nullptr || ! root->hasProperty("models") || ! root->getProperty("models").isArray())
    {
        DBG_AND_LOG("ModelRegistry::getFeaturedModelPaths: Registry is missing a valid models "
                    "array. Using fallback list.");

        return getFallbackFeaturedModelPaths();
    }

    std::vector<std::string> featuredPaths;

    for (const auto& modelVar : *root->getProperty("models").getArray())
    {
        if (! modelVar.isObject())
            continue;

        auto* model = modelVar.getDynamicObject();

        if (model == nullptr)
            continue;

        const bool isFeatured = static_cast<bool>(model->getProperty("featured"));
        const String path = model->getProperty("path").toString();

        if (isFeatured && path.isNotEmpty())
            featuredPaths.push_back(path.toStdString());
    }

    if (featuredPaths.empty())
    {
        DBG_AND_LOG("ModelRegistry::getFeaturedModelPaths: Registry did not yield any featured "
                    "paths. Using fallback list.");

        return getFallbackFeaturedModelPaths();
    }

    return featuredPaths;
}
} // namespace ModelRegistry