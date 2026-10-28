/**
 * @file Model.h
 * @brief Model state and interface for loading and processing.
 * @author cwitkowitz, saumya-pailwan, xribene, hugofloresgarcia, aldo-aguilar
 */

#pragma once

#include <atomic>
#include <string>
#include <vector>

#include <juce_core/juce_core.h>

#include "clients/Client.h"

#include "widgets/StatusAreaWidget.h"

#include "utils/Clients.h"
#include "utils/Controls.h"
#include "utils/Enums.h"
#include "utils/Errors.h"
#include "utils/Labels.h"
#include "utils/Logging.h"

using namespace juce;

// Shown in the status history as the model moves through loading and processing
enum class ModelStatus
{
    EMPTY, // No model has been successfully added since initialization

    QUERYING_CONTROLS, // Controls information for a model is being requested
    PREPARING_REQUEST, // Files and metadata are being prepared for processing
    PROCESSING, // Awaiting result of processing request
    CANCELING, // Awaiting result of cancellation request

    READY // Model is loaded and ready for processing
};

struct ModelMetadata
{
    std::string name;
    std::string author;
    std::string description;

    std::vector<std::string> tags;

    ModelMetadata() {}

    ModelMetadata(DynamicObject* card)
    {
        readString(card, "name", name);
        readString(card, "author", author);
        readString(card, "description", description);
        readStringList(card, "tags", tags);
    }

    // The card as JSON, in the form the constructor reads
    var toVar() const
    {
        Array<var> tagList;

        for (const auto& tag : tags)
            tagList.add(String(tag));

        DynamicObject::Ptr card = new DynamicObject();
        card->setProperty("name", String(name));
        card->setProperty("author", String(author));
        card->setProperty("description", String(description));
        card->setProperty("tags", tagList);

        return var(card.get());
    }
};

class Model
{
public:
    Model() { resetState(/*suppressStatus=*/true); }
    ~Model() { resetState(/*suppressStatus=*/true); }

    bool isEmpty() { return loadedPath.isEmpty() || client == nullptr; }
    bool isLoaded() { return ! isEmpty(); }

    /* A model whose requests were aborted is being discarded, so nothing it reports
       reaches the status area afterwards (see abortActiveRequests) */
    void setStatus(ModelStatus newStatus, const String& pathContext = "")
    {
        if (requestRegistry.hasBeenAborted())
            return;

        statusMessage->setMessage(enumToString(newStatus) + describePath(pathContext));
    }

    void setFailure(const OpResult& result, const String& pathContext = "")
    {
        /* Record the classification alongside the raw traffic already logged by the
           client, so that a log attached to a report shows which branch was taken */
        DBG_AND_LOG("Model::setFailure: " + toLogString(result.getError())
                    + (pathContext.isNotEmpty() ? " for \"" + pathContext + "\"" : String()));

        if (requestRegistry.hasBeenAborted())
            return;

        const String errorMessage = toUserMessage(result.getError()).trim();

        // Only the first line, so that the status history stays readable
        statusMessage->setMessage("[error] "
                                  + errorMessage.upToFirstOccurrenceOf("\n", false, false).trim()
                                  + describePath(pathContext));
    }

    String getLoadedPath() { return loadedPath; }
    String getOpenablePath() { return openablePath; }

    ModelMetadata getMetadata() { return metadata; }
    ModelComponentInfoList getControls() { return controlComponents; }
    ModelComponentInfoList getInputTracks() { return inputTrackComponents; }
    ModelComponentInfoList getOutputTracks() { return outputTrackComponents; }

    void resetState(bool suppressStatus = false)
    {
        metadata = ModelMetadata {};

        orderedInputComponentIDs.clear();

        controlComponents.clear();
        inputTrackComponents.clear();
        outputTrackComponents.clear();

        client.reset();

        loadedPath.clear();
        openablePath.clear();

        if (! suppressStatus)
            setStatus(ModelStatus::EMPTY);
    }

    OpResult load(String pathToLoad)
    {
        // Initialize appropriate client for selected path
        std::unique_ptr<Client> tempClient = createClientForPath(pathToLoad);

        if (tempClient == nullptr)
        {
            const OpResult result =
                OpResult::fail(ClientError { ClientError::Type::UnknownClient, pathToLoad });

            setFailure(result, pathToLoad);

            return result;
        }

        // Requests made while loading belong to this model as well (so that they can be
        // aborted), even though the client only replaces the current one once loading
        // has succeeded
        tempClient->setRequestRegistry(&requestRegistry);

        /* Settle on the address the provider itself uses before anything is queried
           or recorded, so that the same model entered several ways is loaded, listed,
           and linked to as one. */
        String canonicalPath;

        OpResult result = tempClient->resolveCanonicalPath(pathToLoad, canonicalPath);

        if (result.failed())
        {
            setFailure(result, pathToLoad);

            return result;
        }

        pathToLoad = canonicalPath;

        setStatus(ModelStatus::QUERYING_CONTROLS, pathToLoad);

        // Initialize empty dictionary to hold query response
        DynamicObject::Ptr controls;

        // Obtain JSON string corresponding to controls + track layout
        result = tempClient->queryControls(pathToLoad, controls);

        ModelMetadata newMetadata;
        ModelComponentInfoList newInputs;
        ModelComponentInfoList newControls;
        ModelComponentInfoList newOutputs;
        std::vector<Uuid> newInputOrder;

        if (result.wasOk())
            result = extractMetadata(controls, newMetadata);

        if (result.wasOk())
            result = extractInputs(controls, newInputs, newControls, newInputOrder);

        if (result.wasOk())
            result = extractOutputs(controls, newOutputs);

        if (result.failed())
        {
            setFailure(result, pathToLoad);

            return result;
        }

        resetState(/*suppressStatus=*/true);

        // Update model information if all loading operations are successful
        metadata = newMetadata;
        controlComponents = newControls;
        inputTrackComponents = newInputs;
        outputTrackComponents = newOutputs;
        orderedInputComponentIDs = std::move(newInputOrder);

        // Replace existing client
        client = std::move(tempClient);
        // Keep track of successfully loaded path
        loadedPath = pathToLoad;
        openablePath = client->inferDocumentationPath(loadedPath);

        setStatus(ModelStatus::READY, loadedPath);

        return OpResult::ok();
    }

    /* The page of a Gradio app sends a whole number as one, as JavaScript writes it, and
       a model may count or index with it, so a slider or number box at a whole value is sent
       the same way here. JUCE would otherwise write "32.0", which Python reads as a float
       and which fails wherever an int is required. */
    static var numericValue(double value)
    {
        if (exactlyEqual(std::floor(value), value) && std::abs(value) < 1.0e9)
            return var(static_cast<int>(value));

        return var(value);
    }

    OpResult process(const std::map<Uuid, File>& inputFiles,
                     std::vector<File>& outputFiles,
                     LabelList& labels)
    {
        /* A canceled process runs on until the server stops it, which it reports as a
           failure. The cancel has already been reported by then, so whatever this process
           reports afterwards is left out of the status area and only logged. */
        const uint64_t cancelsAtStart = numCancels.load();

        auto reportStatus = [&](ModelStatus newStatus)
        {
            if (numCancels.load() == cancelsAtStart)
                setStatus(newStatus, loadedPath);
        };

        auto reportFailure = [&](const OpResult& result)
        {
            if (numCancels.load() == cancelsAtStart)
                setFailure(result, loadedPath);
            else
                DBG_AND_LOG("Model::process: Canceled process ended with "
                            + toLogString(result.getError()) + ".");
        };

        reportStatus(ModelStatus::PREPARING_REQUEST);

        /* Upload files to the server if necessary and obtain their remote paths. These are
           kept here rather than in the components, which a request still running after a
           cancel shares with the next one. */
        std::map<Uuid, String> remotePaths;

        auto upload = [&](const Uuid& componentID, const File& file)
        {
            String remotePath;

            OpResult result = client->uploadFile(loadedPath, file, remotePath);

            if (result.wasOk())
                remotePaths[componentID] = remotePath;
            else
                reportFailure(result);

            return result;
        };

        for (const auto& [trackID, file] : inputFiles)
        {
            if (OpResult result = upload(trackID, file); result.failed())
                return result;
        }

        for (const auto& controlComponent : controlComponents)
        {
            auto* fileInfo = dynamic_cast<FileComponentInfo*>(controlComponent.get());

            if (fileInfo == nullptr || fileInfo->path.empty())
                continue;

            if (OpResult result = upload(fileInfo->id, File(fileInfo->path)); result.failed())
                return result;
        }

        /* Extract control values for JSON payload in order */

        Array<var> controlValues;

        for (const auto& componentID : orderedInputComponentIDs)
        {
            auto componentInfo = getComponentInfo(componentID);

            // The order is recorded from the same response as the components
            jassert(componentInfo != nullptr);

            if (componentInfo == nullptr)
                continue;

            var controlValue;
            bool isFile = false;

            if (dynamic_cast<TrackComponentInfo*>(componentInfo.get()) != nullptr
                || dynamic_cast<FileComponentInfo*>(componentInfo.get()) != nullptr)
            {
                isFile = true;

                // An optional input left empty is sent as nothing at all
                if (auto it = remotePaths.find(componentID); it != remotePaths.end())
                {
                    DynamicObject::Ptr fileObj = new DynamicObject();

                    fileObj->setProperty("path", it->second);

                    controlValue = var(fileObj.get());
                }
            }
            else if (auto textBoxComponentInfo =
                         dynamic_cast<TextBoxComponentInfo*>(componentInfo.get()))
            {
                controlValue = var(textBoxComponentInfo->value);
            }
            else if (auto numberBoxComponentInfo =
                         dynamic_cast<NumberBoxComponentInfo*>(componentInfo.get()))
            {
                controlValue = numericValue(numberBoxComponentInfo->value);
            }
            else if (auto toggleComponentInfo =
                         dynamic_cast<ToggleComponentInfo*>(componentInfo.get()))
            {
                controlValue = var(toggleComponentInfo->value);
            }
            else if (auto sliderComponentInfo =
                         dynamic_cast<SliderComponentInfo*>(componentInfo.get()))
            {
                controlValue = numericValue(sliderComponentInfo->value);
            }
            else if (auto comboBoxComponentInfo =
                         dynamic_cast<ComboBoxComponentInfo*>(componentInfo.get()))
            {
                controlValue = var(comboBoxComponentInfo->value);
            }
            else if (auto multiSelectComponentInfo =
                         dynamic_cast<MultiSelectComponentInfo*>(componentInfo.get()))
            {
                // Gradio expects the selection of a multiselect dropdown as a list
                Array<var> selectedValues;

                for (const auto& selectedValue : multiSelectComponentInfo->values)
                {
                    selectedValues.add(var(String(selectedValue)));
                }

                controlValue = var(selectedValues);
            }

            controlValues.add(
                client->wrapPayloadElement(controlValue, isFile, String(componentInfo->label)));
        }

        DynamicObject::Ptr dataObject = new DynamicObject();
        dataObject->setProperty("data", controlValues);

        String payloadJSON = JSON::toString(var(dataObject.get()), true);

        DBG_AND_LOG("Model::process: Payload \"" + payloadJSON + "\" prepared for processing.");

        reportStatus(ModelStatus::PROCESSING);

        OpResult result = client->process(loadedPath, payloadJSON, outputFiles, labels);

        if (result.failed())
        {
            reportFailure(result);

            return result;
        }

        reportStatus(ModelStatus::READY);

        return result;
    }

    // Aborts any request this model currently has in flight, locally. Unlike
    // cancel(), this needs neither the server nor a reachable network, and it
    // unblocks the worker thread that is waiting on the connection. Once aborted
    // the model will not open any further connections or report to the status
    // area, so this is only for models that are being discarded (see
    // ModelTab::abandon).
    void abortActiveRequests() { requestRegistry.abortActiveRequests(); }

    OpResult cancel()
    {
        // Counted first, so that the process being canceled reports nothing further
        ++numCancels;

        setStatus(ModelStatus::CANCELING, loadedPath);

        OpResult result = client->cancel(loadedPath);

        if (result.failed())
        {
            setFailure(result, loadedPath);

            return result;
        }

        setStatus(ModelStatus::READY, loadedPath);

        return OpResult::ok();
    }

private:
    // The last part of a path, appended to a status message
    static String describePath(const String& path)
    {
        const String shortPath = path.fromLastOccurrenceOf("/", false, false).trim();

        return shortPath.isNotEmpty() ? " | " + shortPath : String();
    }

    OpResult extractMetadata(DynamicObject::Ptr& controls, ModelMetadata& newMetadata)
    {
        static const Identifier metadataKey { "card" };

        DynamicObject::Ptr metadataDict;

        OpResult result = getRequiredDictProperty(controls, metadataKey, metadataDict);

        if (result.failed())
        {
            return result;
        }

        newMetadata = ModelMetadata(metadataDict.get());

        return OpResult::ok();
    }

    /* The component a "type" in a /controls response declares, or nullptr for a type
       HARP does not support */
    static std::shared_ptr<ModelComponentInfo> createComponentInfo(const String& type,
                                                                   DynamicObject* spec)
    {
        if (type == "audio_track")
            return std::make_shared<AudioTrackComponentInfo>(spec);

        if (type == "midi_track")
            return std::make_shared<MidiTrackComponentInfo>(spec);

        if (type == "generic_file")
            return std::make_shared<FileComponentInfo>(spec);

        if (type == "text_box")
            return std::make_shared<TextBoxComponentInfo>(spec);

        if (type == "number_box")
            return std::make_shared<NumberBoxComponentInfo>(spec);

        if (type == "toggle")
            return std::make_shared<ToggleComponentInfo>(spec);

        if (type == "slider")
            return std::make_shared<SliderComponentInfo>(spec);

        if (type == "dropdown")
        {
            /* A dropdown declared with multiselect=True carries a list of values rather
               than a single one, and needs its own control */
            bool isMultiSelect = false;

            readBool(spec, "multiselect", isMultiSelect);

            if (isMultiSelect)
                return std::make_shared<MultiSelectComponentInfo>(spec);

            return std::make_shared<ComboBoxComponentInfo>(spec);
        }

        return nullptr;
    }

    /* Calls handleComponent with the type and specification of each component listed
       under a key of a /controls response, stopping at the first failure */
    static OpResult readComponents(
        DynamicObject::Ptr& controls,
        const Identifier& key,
        const std::function<OpResult(const String&, DynamicObject*)>& handleComponent)
    {
        static const Identifier typeKey { "type" };

        Array<var>* components;

        OpResult result = getRequiredArrayProperty(controls, key, components);

        if (result.failed())
        {
            return result;
        }

        for (const auto& componentVar : *components)
        {
            auto* spec = componentVar.getDynamicObject();

            if (spec == nullptr)
            {
                return OpResult::fail(
                    JsonError { JsonError::Type::NotADictionary, componentVar.toString() });
            }

            if (! spec->hasProperty(typeKey))
            {
                return OpResult::fail(JsonError { JsonError::Type::MissingKey,
                                                  JSON::toString(componentVar, true),
                                                  typeKey.toString() });
            }

            result = handleComponent(spec->getProperty(typeKey).toString(), spec);

            if (result.failed())
            {
                return result;
            }
        }

        return OpResult::ok();
    }

    // Tracks are inputs, and anything else is a control, which are all sent in this order
    static OpResult extractInputs(DynamicObject::Ptr& controls,
                                  ModelComponentInfoList& newInputs,
                                  ModelComponentInfoList& newControls,
                                  std::vector<Uuid>& newInputOrder)
    {
        return readComponents(
            controls,
            "inputs",
            [&](const String& type, DynamicObject* spec)
            {
                auto info = createComponentInfo(type, spec);

                if (info == nullptr)
                {
                    return OpResult::fail(
                        ControlError { ControlError::Type::UnsupportedControl, type });
                }

                const bool isTrack = dynamic_cast<TrackComponentInfo*>(info.get()) != nullptr;

                (isTrack ? newInputs : newControls).push_back(info);
                newInputOrder.push_back(info->id);

                DBG_AND_LOG("Model::extractInputs: " << (isTrack ? "Track" : "Control") << " \""
                                                     << String(info->label) << "\" of type \""
                                                     << type << "\" extracted.");

                return OpResult::ok();
            });
    }

    static OpResult extractOutputs(DynamicObject::Ptr& controls, ModelComponentInfoList& newOutputs)
    {
        return readComponents(
            controls,
            "outputs",
            [&](const String& type, DynamicObject* spec)
            {
                // Labels arrive with the other outputs and are drawn on their tracks
                if (type == "json")
                {
                    DBG_AND_LOG("Model::extractOutputs: JSON (labels) output detected.");

                    return OpResult::ok();
                }

                const bool isOutputType =
                    type == "audio_track" || type == "midi_track" || type == "generic_file";

                auto info = isOutputType ? createComponentInfo(type, spec) : nullptr;

                if (info == nullptr)
                {
                    return OpResult::fail(
                        ControlError { ControlError::Type::UnsupportedControl, type });
                }

                newOutputs.push_back(info);

                DBG_AND_LOG("Model::extractOutputs: Output \""
                            << String(info->label) << "\" of type \"" << type << "\" extracted.");

                return OpResult::ok();
            });
    }

    std::shared_ptr<ModelComponentInfo> getComponentInfo(const Uuid& id) const
    {
        for (const auto* list :
             { &controlComponents, &inputTrackComponents, &outputTrackComponents })
        {
            for (const auto& component : *list)
            {
                if (component->id == id)
                {
                    return component;
                }
            }
        }

        return nullptr;
    }

    // Declared before the client so that it is destroyed after it
    RequestRegistry requestRegistry;

    // Advanced by every cancel, so that a process can tell it has been canceled
    std::atomic<uint64_t> numCancels { 0 };

    std::unique_ptr<Client> client;

    String loadedPath;
    String openablePath;

    ModelMetadata metadata;

    std::vector<Uuid> orderedInputComponentIDs;

    ModelComponentInfoList controlComponents;

    ModelComponentInfoList inputTrackComponents;
    ModelComponentInfoList outputTrackComponents;

    SharedResourcePointer<StatusMessage> statusMessage;
};
