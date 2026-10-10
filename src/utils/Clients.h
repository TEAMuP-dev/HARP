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

inline std::unique_ptr<Client> multiplexClients(Provider provider)
{
    if (provider == Provider::Stability)
    {
        DBG_AND_LOG("utils::multiplexClients: Initializing Stability client.");

        return std::make_unique<StabilityClient>();
    }
    else
    {
        DBG_AND_LOG("utils::multiplexClients: Initializing Gradio client.");

        return std::make_unique<GradioClient>();
    }
}

/**
 * Reduces a model path to the single form its provider recognizes it by.
 *
 * Returns the path unchanged when no provider claims it, so that an unrecognized
 * path still reaches the client selection above and fails there with a message
 * about the path itself.
 */
inline String canonicalizeModelPath(String modelPath)
{
    std::unique_ptr<Client> client;

    if (StabilityClient::matchesPathSpec(modelPath))
    {
        client = std::make_unique<StabilityClient>();
    }
    else if (GradioClient::matchesPathSpec(modelPath))
    {
        client = std::make_unique<GradioClient>();
    }

    return client != nullptr ? client->canonicalizePath(modelPath) : modelPath;
}

inline OpResult multiplexClients(String modelPath, std::unique_ptr<Client>& client)
{
    if (StabilityClient::matchesPathSpec(modelPath))
    {
        DBG_AND_LOG(
            "utils::multiplexClients: Stability AI path detected. Initializing Stability client.");

        client = std::make_unique<StabilityClient>();
    }
    // Check Gradio last to serve as a catch-all if no third-party provider path specifications match
    else if (GradioClient::matchesPathSpec(modelPath))
    {
        DBG_AND_LOG("utils::multiplexClients: Gradio path detected. Initializing Gradio client.");

        client = std::make_unique<GradioClient>();
    }
    else
    {
        ClientError error { ClientError::Type::UnknownClient, modelPath, {} };

        DBG_AND_LOG("utils::multiplexClients: " << toUserMessage(error));

        return OpResult::fail(error);
    }

    return OpResult::ok();
}

/**
 * Whether any provider recognizes a model path, so that one no client could load can be
 * rejected before anything is attempted with it.
 */
inline bool isSupportedModelPath(const String& modelPath)
{
    return StabilityClient::matchesPathSpec(modelPath) || GradioClient::matchesPathSpec(modelPath);
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
