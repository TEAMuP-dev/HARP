/**
 * @file ModelCatalog.h
 * @brief The models HARP offers for browsing on the Home tab.
 *
 * The catalog combines the models whose controls ship with HARP (Stability AI), every
 * Space of the HARP organization on Hugging Face, and any other paths the user has loaded
 * successfully. The organization's Spaces are listed through the Hub API in a single
 * request, which describes each Space (including its tags, see ModelTags.h) without waking
 * any of them. The last listing is cached on disk, so the Home tab is populated at once on
 * startup and still works offline.
 */

#pragma once

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include <juce_events/juce_events.h>

#include "../Model.h"

#include "Clients.h"
#include "Errors.h"
#include "Logging.h"
#include "ModelTags.h"
#include "Settings.h"

using namespace juce;

struct CatalogEntry
{
    // Outcome of the last attempt to load a model in this session
    enum class LoadStatus
    {
        None, // Not attempted, or loaded successfully
        Failed, // Failed for a reason specific to the model
        Unavailable, // The Space is not running (e.g., it crashed or was paused)
        TryAgain // Failed for a reason that is likely temporary
    };

    // A short note on whether the model can be expected to load
    struct AvailabilityNote
    {
        String text; // Empty if there is nothing to note
        bool isProblem = false; // As opposed to merely informative, e.g. "Sleeping"
    };

    String path;
    String name;
    String description;
    String provider;
    ModelTags tags;

    // Deployment details reported by the Hub, empty when unknown
    String stage; // e.g. "RUNNING", "SLEEPING", "RUNTIME_ERROR"
    String hardware; // e.g. "cpu-basic", "zero-a10g"

    bool isCustom = false; // Added by the user rather than found
    LoadStatus loadStatus = LoadStatus::None;

    bool isZeroGPU() const { return hardware.startsWithIgnoreCase("zero-"); }

    /* The Hub's report on a Space, before anything is asked of it, is only an expectation.
       Once a load has been attempted, its outcome is what counts. */
    AvailabilityNote getAvailabilityNote() const
    {
        switch (loadStatus)
        {
            case LoadStatus::Failed: // Only custom paths are still listed after this
                return { "Failed to load", true };
            case LoadStatus::Unavailable:
                return { "Unavailable", true };
            case LoadStatus::TryAgain:
                return { "Try again", true };
            case LoadStatus::None:
                break;
        }

        if (stage == "SLEEPING")
            return { "Sleeping" }; // Loads, after a wait while the Space starts
        if (stage.contains("BUILDING") || stage.contains("STARTING"))
            return { "Starting" };

        return {};
    }

    // Whether the Hub reports the Space as unable to serve at all, e.g. crashed or paused
    bool isReportedBroken() const
    {
        return stage.contains("ERROR") || stage == "PAUSED" || stage == "STOPPED"
               || stage == "NO_APP_FILE" || stage == "DELETING";
    }

    String getSearchableText() const
    {
        return (name + " " + description + " " + path + " " + provider + " "
                + tags.getSearchableText())
            .toLowerCase();
    }
};

class ModelCatalog : public ChangeBroadcaster
{
public:
    // Hugging Face organization whose Spaces are listed
    static constexpr const char* hubOrganization = "teamup-tech";

    enum class FetchState
    {
        Fetching,
        Succeeded,
        Failed
    };

    ModelCatalog()
    {
        for (const auto& [path, card] : getBuiltInModelCards())
            builtInEntries.push_back(makeEntry(path, ModelMetadata(card.get()), "Stability AI"));

        customPaths = StringArray::fromLines(Settings::getString(customPathsKey));
        customPaths.removeEmptyStrings();

        loadCachedListing();
        rebuildEntries();

        refresh();
    }

    ~ModelCatalog() override
    {
        /* A listing in flight would leave its worker blocked in a network call, and
           destroying the pool under it would force-kill the thread. Abort the request so
           the worker returns at once, then wait for it. */
        requestRegistry.abortActiveRequests();
        fetchPool.removeAllJobs(true, 5000);
    }

    // All entries, built-in models first, then the organization's, then custom paths
    const std::vector<CatalogEntry>& getEntries() const { return entries; }

    const CatalogEntry* findEntry(const String& path) const
    {
        for (const auto& entry : entries)
        {
            if (entry.path.equalsIgnoreCase(path))
                return &entry;
        }

        return nullptr;
    }

    // Lists the organization's Spaces again, in the background
    void refresh()
    {
        if (fetchState == FetchState::Fetching && fetchPool.getNumJobs() > 0)
            return;

        fetchState = FetchState::Fetching;
        sendChangeMessage();

        WeakReference<ModelCatalog> weakThis(this);

        fetchPool.addJob(
            [this, weakThis]
            {
                auto listing = std::make_shared<var>();
                String error;

                const bool succeeded = fetchHubListing(*listing, error);

                MessageManager::callAsync(
                    [weakThis, succeeded, listing, error]
                    {
                        if (auto* catalog = weakThis.get())
                            catalog->finishRefresh(succeeded, *listing, error);
                    });
            });
    }

    FetchState getFetchState() const { return fetchState; }
    String getFetchError() const { return fetchError; }
    // When the listing being shown was obtained, or a null time if there is none
    Time getListingTime() const { return listingTime; }
    int getNumHubEntries() const { return (int) (hubEntries.size() - hiddenEntries.size()); }

    // Spaces left out because they cannot currently be loaded, with the reason for each
    struct HiddenEntry
    {
        String name;
        String reason;
    };

    const std::vector<HiddenEntry>& getHiddenEntries() const { return hiddenEntries; }

    static String getHubOrganizationURL()
    {
        return "https://huggingface.co/" + String(hubOrganization);
    }

    /* A loaded model's own card is more authoritative than what its listing says about it,
       so it is shown in the listing's place for the rest of the session. */
    void recordLoadSuccess(const String& path, const ModelMetadata& card)
    {
        loadStatuses.erase(path.toLowerCase());
        loadedCards[path.toLowerCase()] = card;

        if (findEntry(path) == nullptr)
        {
            customPaths.add(path);
            saveCustomPaths();
        }

        rebuildEntries();
        sendChangeMessage();
    }

    void recordLoadFailure(const String& path, const Error& error)
    {
        // Paths that never loaded are not listed, so a typo does not become an entry
        if (findEntry(path) == nullptr)
            return;

        loadStatuses[path.toLowerCase()] = classifyLoadFailure(error);

        rebuildEntries();
        sendChangeMessage();
    }

    void removeCustomPath(const String& path)
    {
        customPaths.removeString(path, true);
        saveCustomPaths();

        rebuildEntries();
        sendChangeMessage();
    }

    /* What a failed load says about the model:
         - TryAgain: nothing about the model itself, e.g. the connection dropped, the request
           timed out or was rate limited, the GPU quota ran out, or the Space was still
           waking up. The same load is likely to work later.
         - Unavailable: the Space is not running, e.g. it crashed or was paused.
         - Failed: the Space answered, but not as a HARP model, e.g. it does not exist, is
           private, or its interface could not be read. Retrying will not help. */
    static CatalogEntry::LoadStatus classifyLoadFailure(const Error& error)
    {
        using LoadStatus = CatalogEntry::LoadStatus;

        if (const auto* httpError = std::get_if<HttpError>(&error))
        {
            if (httpError->type == HttpError::Type::ConnectionFailed)
                return LoadStatus::TryAgain;

            if (httpError->type == HttpError::Type::BadStatusCode && httpError->statusCode == 429)
                return LoadStatus::TryAgain;

            // Only reaches here when the Hub could not say why (see GradioClient)
            if (httpError->type == HttpError::Type::BadStatusCode && httpError->statusCode == 503)
                return LoadStatus::Unavailable;
        }
        else if (const auto* gradioError = std::get_if<GradioError>(&error))
        {
            switch (gradioError->type)
            {
                case GradioError::Type::SpaceStarting:
                case GradioError::Type::IncompleteResponse:
                case GradioError::Type::QuotaExceeded:
                    return LoadStatus::TryAgain;

                case GradioError::Type::SpaceUnavailable:
                    return LoadStatus::Unavailable;

                case GradioError::Type::RuntimeError:
                case GradioError::Type::Indeterminate:
                    break;
            }
        }

        return LoadStatus::Failed;
    }

private:
    static CatalogEntry
        makeEntry(const String& path, const ModelMetadata& metadata, const String& provider)
    {
        CatalogEntry entry;
        entry.path = path;
        entry.name = metadata.name.empty() ? getNameFromPath(path) : String(metadata.name);
        entry.description = metadata.description;
        entry.provider = provider;

        StringArray tags;

        for (const auto& tag : metadata.tags)
            tags.add(tag);

        entry.tags = ModelTags::parse(tags);

        return entry;
    }

    static String getNameFromPath(const String& path)
    {
        return path.fromLastOccurrenceOf("/", false, false).replaceCharacters("-_", "  ").trim();
    }

    /* The Hub pages its results, so the request is repeated for as long as the response
       links to a next page. Runs on the fetch thread. */
    bool fetchHubListing(var& listing, String& error)
    {
        static constexpr int timeoutMs = 15000;
        static constexpr int maxPages = 20;

        URL pageURL = URL("https://huggingface.co/api/spaces")
                          .withParameter("author", hubOrganization)
                          .withParameter("limit", "1000")
                          .withParameter("expand[]", "cardData")
                          .withParameter("expand[]", "runtime")
                          .withParameter("expand[]", "private")
                          .withParameter("expand[]", "disabled");

        Array<var> spaces;

        for (int page = 0; page < maxPages && pageURL.isWellFormed(); ++page)
        {
            DBG_AND_LOG("ModelCatalog::fetchHubListing: Requesting \"" << pageURL.toString(true)
                                                                       << "\".");

            int statusCode = 0;
            StringPairArray responseHeaders;

            auto options = URL::InputStreamOptions(URL::ParameterHandling::inAddress)
                               .withConnectionTimeoutMs(timeoutMs)
                               .withStatusCode(&statusCode)
                               .withResponseHeaders(&responseHeaders)
                               .withNumRedirectsToFollow(5);

            auto stream = createRegisteredStream(&requestRegistry, pageURL, options);

            if (stream == nullptr)
            {
                error = requestRegistry.hasBeenAborted() ? "the request was aborted"
                                                         : "could not connect";
                return false;
            }

            if (statusCode != 200)
            {
                error = "status code " + String(statusCode);
                return false;
            }

            const var response = JSON::parse(stream->readEntireStreamAsString());

            if (! response.isArray())
            {
                error = "unexpected response";
                return false;
            }

            spaces.addArray(*response.getArray());

            pageURL = URL(getNextPageLink(responseHeaders));
        }

        listing = spaces;

        DBG_AND_LOG("ModelCatalog::fetchHubListing: Found " << spaces.size() << " Spaces.");

        return true;
    }

    // The target of a 'Link: <...>; rel="next"' header, or empty if there is none
    static String getNextPageLink(const StringPairArray& headers)
    {
        for (const auto& link : StringArray::fromTokens(headers["link"], ",", "<>"))
        {
            if (link.contains("rel=\"next\""))
                return link.fromFirstOccurrenceOf("<", false, false)
                    .upToFirstOccurrenceOf(">", false, false)
                    .trim();
        }

        return {};
    }

    void finishRefresh(bool succeeded, const var& listing, const String& error)
    {
        if (succeeded)
        {
            hubEntries = parseHubListing(listing, true);
            listingTime = Time::getCurrentTime();
            fetchState = FetchState::Succeeded;
            fetchError.clear();

            saveCachedListing(listing);
        }
        else
        {
            DBG_AND_LOG("ModelCatalog::finishRefresh: Could not list models (" << error << ").");

            fetchState = FetchState::Failed;
            fetchError = error;
        }

        rebuildEntries();
        sendChangeMessage();
    }

    std::vector<CatalogEntry> parseHubListing(const var& listing, bool isCurrent) const
    {
        std::vector<CatalogEntry> parsed;

        if (auto* spaces = listing.getArray())
        {
            for (const auto& space : *spaces)
            {
                const String path = space["id"].toString();

                // Hidden from anyone without access, or switched off by the Hub
                if (path.isEmpty() || (bool) space["private"] || (bool) space["disabled"])
                    continue;

                const var& cardData = space["cardData"];

                StringArray tags;

                if (auto* tagList = cardData["tags"].getArray())
                {
                    for (const auto& tag : *tagList)
                        tags.add(tag.toString());
                }

                CatalogEntry entry;
                entry.path = path;
                entry.name = cardData["title"].toString().trim();
                // A summary of the model card's description, which the Space's README is
                // recommended to carry (see "Listing on the Home Tab" in the pyharp README).
                // The card itself replaces it once the model is loaded.
                entry.description = cardData["short_description"].toString().trim();
                entry.provider = "Hugging Face";
                entry.tags = ModelTags::parse(tags);
                entry.hardware = space["runtime"]["hardware"]["current"].toString();

                // A cached stage is stale, and would misreport whether the Space is awake
                if (isCurrent)
                    entry.stage = space["runtime"]["stage"].toString().toUpperCase();

                if (entry.name.isEmpty())
                    entry.name = getNameFromPath(path);

                parsed.push_back(std::move(entry));
            }
        }

        std::sort(parsed.begin(),
                  parsed.end(),
                  [](const CatalogEntry& a, const CatalogEntry& b)
                  { return a.name.compareNatural(b.name) < 0; });

        return parsed;
    }

    void rebuildEntries()
    {
        entries = builtInEntries;
        hiddenEntries.clear();

        /* A Space that cannot load is not offered, whether the Hub reports it as broken or
           it answered a load this session in a way no retry will change (e.g., it is not a
           HARP app, or needs controls this version does not support). Custom paths are the
           user's own, so they stay listed with a note instead. */
        for (const auto& entry : hubEntries)
        {
            auto status = loadStatuses.find(entry.path.toLowerCase());

            const bool failedToLoad = status != loadStatuses.end()
                                      && status->second == CatalogEntry::LoadStatus::Failed;

            if (failedToLoad)
                hiddenEntries.push_back({ entry.name, "failed to load" });
            else if (entry.isReportedBroken())
                hiddenEntries.push_back(
                    { entry.name, "the Space reports " + entry.stage.replace("_", " ").toLowerCase() });
            else
                entries.push_back(entry);
        }

        for (const auto& path : customPaths)
        {
            if (findEntry(path) != nullptr)
                continue;

            CatalogEntry entry;
            entry.path = path;
            entry.name = getNameFromPath(path);
            entry.provider = "Custom";
            entry.isCustom = true;

            entries.push_back(std::move(entry));
        }

        for (auto& entry : entries)
        {
            const String key = entry.path.toLowerCase();

            auto status = loadStatuses.find(key);

            entry.loadStatus =
                status != loadStatuses.end() ? status->second : CatalogEntry::LoadStatus::None;

            if (auto loaded = loadedCards.find(key); loaded != loadedCards.end())
            {
                const ModelMetadata& card = loaded->second;

                if (! card.description.empty())
                    entry.description = card.description;

                if (entry.isCustom && ! card.name.empty())
                    entry.name = card.name;

                if (! card.tags.empty())
                {
                    StringArray tags;

                    for (const auto& tag : card.tags)
                        tags.add(tag);

                    entry.tags = ModelTags::parse(tags);
                }
            }
        }
    }

    static File getCacheFile()
    {
        if (auto* settings = Settings::getUserSettings())
            return settings->getFile().getSiblingFile("model_catalog.json");

        return {};
    }

    void loadCachedListing()
    {
        const File cacheFile = getCacheFile();

        if (! cacheFile.existsAsFile())
            return;

        hubEntries = parseHubListing(JSON::parse(cacheFile), false);
        listingTime = cacheFile.getLastModificationTime();
    }

    static void saveCachedListing(const var& listing)
    {
        const File cacheFile = getCacheFile();

        if (cacheFile != File() && ! cacheFile.replaceWithText(JSON::toString(listing, true)))
        {
            DBG_AND_LOG("ModelCatalog::saveCachedListing: Could not write \""
                        << cacheFile.getFullPathName() << "\".");
        }
    }

    void saveCustomPaths()
    {
        Settings::setValue(customPathsKey, customPaths.joinIntoString("\n"), true);
    }

    static constexpr const char* customPathsKey = "models.customPaths";

    std::vector<CatalogEntry> builtInEntries;
    std::vector<CatalogEntry> hubEntries;
    StringArray customPaths;
    // Keyed by lowercase path, since the Hub does not distinguish case
    std::map<String, CatalogEntry::LoadStatus> loadStatuses;
    std::map<String, ModelMetadata> loadedCards;
    std::vector<HiddenEntry> hiddenEntries;

    std::vector<CatalogEntry> entries;

    FetchState fetchState = FetchState::Failed;
    String fetchError;
    Time listingTime;

    RequestRegistry requestRegistry;
    // Declared last so that it is destroyed first, while what its job uses still exists
    ThreadPool fetchPool { 1 };

    JUCE_DECLARE_WEAK_REFERENCEABLE(ModelCatalog)
};
