/**
 * @file ModelRegistry.h
 * @brief Accessors for the bundled model registry.
 */

#pragma once

#include <string>
#include <vector>

#include <BinaryData.h>
#include <juce_core/juce_core.h>

#include "Logging.h"

using namespace juce;

namespace ModelRegistry
{
// The model selection UI treats the first combo-box entry as the "enter a custom
// path" affordance, so the placeholder is prepended here rather than stored in
// the registry, keeping the registry models-only.
inline constexpr const char* customPathPlaceholder = "click here to enter a custom path...";

inline std::vector<std::string> getFeaturedModelPaths()
{
    std::vector<std::string> featuredPaths { customPathPlaceholder };

    const String registryJson =
        String::fromUTF8(BinaryData::model_registry_json, BinaryData::model_registry_jsonSize);

    var parsedRegistry;
    const Result parseResult = JSON::parse(registryJson, parsedRegistry);

    // The bundled registry is validated at configure time (see CMakeLists.txt),
    // so failures here indicate a corrupted build rather than a bad edit.
    if (parseResult.failed() || ! parsedRegistry.isObject())
    {
        DBG_AND_LOG("ModelRegistry::getFeaturedModelPaths: Failed to parse bundled registry.");

        return featuredPaths;
    }

    auto* root = parsedRegistry.getDynamicObject();

    if (root == nullptr || ! root->getProperty("models").isArray())
    {
        DBG_AND_LOG("ModelRegistry::getFeaturedModelPaths: Registry is missing a valid models "
                    "array.");

        return featuredPaths;
    }

    for (const auto& modelVar : *root->getProperty("models").getArray())
    {
        auto* model = modelVar.getDynamicObject();

        if (model == nullptr)
            continue;

        const bool isFeatured = static_cast<bool>(model->getProperty("featured"));
        const String path = model->getProperty("path").toString();

        if (isFeatured && path.isNotEmpty())
            featuredPaths.push_back(path.toStdString());
    }

    return featuredPaths;
}
} // namespace ModelRegistry
