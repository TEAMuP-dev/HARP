/**
 * @file Clients.h
 * @brief Helper functions for creating an appropriate client.
 * @author cwitkowitz
 */

#pragma once

#include <juce_core/juce_core.h>

#include "../clients/Client.h"
#include "../clients/GradioClient.h"
#include "../clients/providers/stability/StabilityClient.h"

#include "Errors.h"
#include "Logging.h"

using namespace juce;

inline std::unique_ptr<Client> createClient(Provider provider)
{
    if (provider == Provider::Stability)
    {
        return std::make_unique<StabilityClient>();
    }

    return std::make_unique<GradioClient>();
}

/**
 * The client of the provider that recognizes a model path, or nullptr if none does.
 *
 * Gradio is checked last, as the catch-all for paths that match no other provider.
 */
inline std::unique_ptr<Client> createClientForPath(const String& modelPath)
{
    if (StabilityClient::matchesPathSpec(modelPath))
    {
        return createClient(Provider::Stability);
    }

    if (GradioClient::matchesPathSpec(modelPath))
    {
        return createClient(Provider::HuggingFace);
    }

    return nullptr;
}

/**
 * Whether any provider recognizes a model path, so that one no client could load can be
 * rejected before anything is attempted with it.
 */
inline bool isSupportedModelPath(const String& modelPath)
{
    return createClientForPath(modelPath) != nullptr;
}

/**
 * Reduces a model path to the single form its provider recognizes it by.
 *
 * Returns the path unchanged when no provider claims it, so that an unrecognized path
 * still fails to load with a message about the path itself.
 */
inline String canonicalizeModelPath(const String& modelPath)
{
    auto client = createClientForPath(modelPath);

    return client != nullptr ? client->canonicalizePath(modelPath) : modelPath;
}

/**
 * The model cards of models whose controls ship with HARP, keyed by model path, so that
 * they can be listed without querying anything.
 */
inline std::vector<std::pair<String, DynamicObject::Ptr>> getBuiltInModelCards()
{
    static const Identifier cardKey { "card" };

    std::vector<std::pair<String, DynamicObject::Ptr>> cards;

    StabilityClient stabilityClient;

    for (const auto& modelPath : StabilityClient::getModelPaths())
    {
        DynamicObject::Ptr controls;

        if (stabilityClient.queryControls(modelPath, controls).wasOk() && controls != nullptr)
        {
            if (auto* card = controls->getProperty(cardKey).getDynamicObject())
                cards.emplace_back(modelPath, card);
        }
    }

    return cards;
}
