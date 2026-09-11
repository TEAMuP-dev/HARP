/**
 * @file Providers.h
 * @brief Identifies the services HARP can reach models through.
 * @author cwitkowitz
 */

#pragma once

/* Kept out of Client.h so that an error can name a provider too. Client.h already
   includes Errors.h, so the enum cannot live there without the two including each
   other. */
enum class Provider
{
    HuggingFace,
    Stability
};
