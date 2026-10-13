/**
 * @file Providers.h
 * @brief Identifies the services HARP can reach models through.
 * @author cwitkowitz
 */

#pragma once

#include <juce_core/juce_core.h>

/* Kept out of Client.h so that an error can name a provider too. Client.h already
   includes Errors.h, so the enum cannot live there without the two including each
   other. */
enum class Provider
{
    HuggingFace,
    Stability
};

// The name of a provider as the user knows it
inline juce::String getProviderName(Provider provider)
{
    switch (provider)
    {
        case Provider::HuggingFace:
            return "Hugging Face";
        case Provider::Stability:
            return "Stability AI";
    }

    return {};
}
