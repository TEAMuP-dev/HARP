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
