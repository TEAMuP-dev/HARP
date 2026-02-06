/**
 * @file Model.h
 * @brief Model state and interface for loading and processing.
 * @author hugofloresgarcia, aldo-aguilar, xribene, cwitkowitz
 */

#pragma once

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

enum class ModelStatus
{
    EMPTY, // No model has been successfully added since initialization

    QUERYING_CONTROLS, // Controls information for a model is being requested
    PREPARING_REQUEST, // Files and metadata is being prepared for processing
    PROCESSING, // Awaiting result of processing request
    CANCELING, // Awaiting result of cancellation request

    FAILURE, // Synonymous with EMPTY or READY but indicates error occurred
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
        // TODO - check that the following properties are of the correct type

        if (card->hasProperty("name"))
        {
            name = card->getProperty("name").toString().toStdString();
        }
        if (card->hasProperty("author"))
        {
            author = card->getProperty("author").toString().toStdString();
        }
        if (card->hasProperty("description"))
        {
            description = card->getProperty("description").toString().toStdString();
        }
        if (card->hasProperty("tags"))
        {
            Array<var>* newTags = card->getProperty("tags").getArray();

            if (newTags == nullptr)
            {
                DBG_AND_LOG("ModelMetadata::ModelMetadata: Failed to parse tags.");
            }
            else
            {
                for (int i = 0; i < newTags->size(); i++)
                {
                    tags.push_back(newTags->getReference(i).toString().toStdString());
                }
            }
        }
    }
};

class Model
{
public:
    Model() { resetState(); }
    ~Model() { resetState(); }

    bool isEmpty() { return loadedPath.isEmpty() || client == nullptr; }
    bool isLoaded() { return ! isEmpty(); }

    void setStatus(ModelStatus newStatus)
    {
        status = newStatus;

        if (statusMessage != nullptr)
        {
            statusMessage->setMessage("Model Status: " + enumToString(status));
        }
    }

    String getLoadedPath() { return loadedPath; }
    String getOpenablePath() { return openablePath; }

    ModelMetadata getMetadata() { return metadata; }
    ModelComponentInfoList getControls() { return controlComponents; }
    ModelComponentInfoList getInputTracks() { return inputTrackComponents; }
    ModelComponentInfoList getOutputTracks() { return outputTrackComponents; }

    void resetState()
    {
        metadata = ModelMetadata {};

        orderedInputComponentIDs.clear();

        controlComponents.clear();
        inputTrackComponents.clear();
        outputTrackComponents.clear();

        client.reset();

        loadedPath.clear();
        openablePath.clear();

        setStatus(ModelStatus::EMPTY);
    }

    OpResult load(String pathToLoad)
    {
        // Create new client for querying
        std::unique_ptr<Client> tempClient;

        // Initialize appropriate client for selected path
        OpResult result = multiplexClients(pathToLoad, tempClient);

        if (result.failed())
        {
            setStatus(ModelStatus::FAILURE);

            return result;
        }

        setStatus(ModelStatus::QUERYING_CONTROLS);

        // Initialize empty dictionary to hold query response
        DynamicObject::Ptr controls;

        // Obtain JSON string corresponding to controls + track layout
        result = tempClient->queryControls(pathToLoad, controls);

        if (result.failed())
        {
            setStatus(ModelStatus::FAILURE);

            return result;
        }

        ModelMetadata newMetadata;

        // Attempt to extract metadata from JSON
        result = extractMetadata(controls, newMetadata);

        if (result.failed())
        {
            setStatus(ModelStatus::FAILURE);

            return result;
        }

        ModelComponentInfoList newInputs;
        ModelComponentInfoList newControls;

        // Attempt to extract inputs and controls from JSON
        result = extractInputs(controls, newInputs, newControls);

        if (result.failed())
        {
            setStatus(ModelStatus::FAILURE);

            return result;
        }

        ModelComponentInfoList newOutputs;

        // Attempt to extract outputs from JSON
        result = extractOutputs(controls, newOutputs);

        if (result.failed())
        {
            setStatus(ModelStatus::FAILURE);

            return result;
        }

        resetState();

        // Update model information if all loading operations are successful
        metadata = newMetadata;
        controlComponents = newControls;
        inputTrackComponents = newInputs;
        outputTrackComponents = newOutputs;

        // Register extracted component IDs
        orderedInputComponentIDs = std::move(tempComponentIDs);
        // Replace existing client
        client = std::move(tempClient);
        // Keep track of successfully loaded path
        loadedPath = pathToLoad;
        openablePath = client->inferDocumentationPath(loadedPath);

        setStatus(ModelStatus::READY);

        return OpResult::ok();
    }

    OpResult
        process(std::map<Uuid, File> inputFiles, std::vector<File>& outputFiles, LabelList& labels)
    {
        /* Upload files to server if necessary and obtain remote paths */

        OpResult result = OpResult::ok();

        setStatus(ModelStatus::PREPARING_REQUEST);

        for (auto& fileEntry : inputFiles)
        {
            String remoteFilePath;

            Uuid trackID = fileEntry.first;
            File fileToUpload = fileEntry.second;

            result = client->uploadFile(loadedPath, fileToUpload, remoteFilePath);

            if (result.failed())
            {
                setStatus(ModelStatus::FAILURE);

                return result;
            }

            auto componentInfo = getComponentInfo(trackID);

            if (componentInfo == nullptr)
            {
                // No components match track ID
                jassertfalse;
            }
            else if (auto trackComponentInfo =
                         dynamic_cast<TrackComponentInfo*>(componentInfo.get()))
            {
                trackComponentInfo->path = remoteFilePath.toStdString();
            }
            else
            {
                // Component is not a valid input track
                jassertfalse;
            }
        }

        /* Extract control values for JSON payload in order */

        Array<var> controlValues;

        for (const auto& componentID : orderedInputComponentIDs)
        {
            auto componentInfo = getComponentInfo(componentID);

            var controlValue;
            bool wasFile = false;

            String label = componentInfo->label;

            if (auto trackComponentInfo = dynamic_cast<TrackComponentInfo*>(componentInfo.get()))
            {
                if (trackComponentInfo->path.empty())
                {
                    controlValue = var(); // Empty
                }
                else
                {
                    DynamicObject::Ptr fileObj = new DynamicObject();

                    fileObj->setProperty("path", var(trackComponentInfo->path));

                    controlValue = var(fileObj);
                }

                wasFile = true;
            }
            else if (auto textBoxComponentInfo =
                         dynamic_cast<TextBoxComponentInfo*>(componentInfo.get()))
            {
                controlValue = var(textBoxComponentInfo->value);
            }
            else if (auto numberBoxComponentInfo =
                         dynamic_cast<NumberBoxComponentInfo*>(componentInfo.get()))
            {
                controlValue = var(numberBoxComponentInfo->value);
            }
            else if (auto toggleComponentInfo =
                         dynamic_cast<ToggleComponentInfo*>(componentInfo.get()))
            {
                controlValue = var(toggleComponentInfo->value);
            }
            else if (auto sliderComponentInfo =
                         dynamic_cast<SliderComponentInfo*>(componentInfo.get()))
            {
                controlValue = var(sliderComponentInfo->value);
            }
            else if (auto comboBoxComponentInfo =
                         dynamic_cast<ComboBoxComponentInfo*>(componentInfo.get()))
            {
                controlValue = var(comboBoxComponentInfo->value);
            }
            else
            {
                // Unsupported control was added
                jassertfalse;
            }

            controlValue = client->wrapPayloadElement(controlValue, wasFile, label);
            controlValues.add(controlValue);
        }

        DynamicObject::Ptr dataObject = new DynamicObject();
        dataObject->setProperty("data", controlValues);

        String payloadJSON = JSON::toString(var(dataObject), true);

        DBG_AND_LOG("Model::process: Payload \"" + payloadJSON + "\" prepared for processing.");

        setStatus(ModelStatus::PROCESSING);

        result = client->process(loadedPath, payloadJSON, outputFiles, labels);

        if (result.failed())
        {
            setStatus(ModelStatus::FAILURE);

            return result;
        }

        setStatus(ModelStatus::READY);

        return result;
    }

    OpResult cancel()
    {
        setStatus(ModelStatus::CANCELING);

        OpResult result = client->cancel(loadedPath);

        if (result.failed())
        {
            setStatus(ModelStatus::FAILURE);

            return result;
        }

        setStatus(ModelStatus::READY);

        return OpResult::ok();
    }

private:
    OpResult extractMetadata(DynamicObject::Ptr& controls, ModelMetadata& newMetadata)
    {
        static const Identifier metadataKey { "card" };

        DynamicObject::Ptr metadataDict;

        OpResult result = getRequiredDictProperty(controls, metadataKey, metadataDict);

        if (result.failed())
        {
            return result;
        }

        newMetadata = ModelMetadata(metadataDict);

        return OpResult::ok();
    }

    OpResult extractInputs(DynamicObject::Ptr& controls,
                           ModelComponentInfoList& newInputs,
                           ModelComponentInfoList& newControls)
    {
        static const Identifier inputsKey { "inputs" };

        Array<var>* inputComponents;

        OpResult result = getRequiredArrayProperty(controls, inputsKey, inputComponents);

        if (result.failed())
        {
            return result;
        }

        for (int i = 0; i < inputComponents->size(); i++)
        {
            var controlsVar = inputComponents->getReference(i);

            if (! controlsVar.isObject())
            {
                return OpResult::fail(
                    JsonError { JsonError::Type::NotADictionary, controlsVar.toString() });
            }

            DynamicObject* controlsDict = controlsVar.getDynamicObject();

            static const Identifier typeKey { "type" };

            // TODO - could abstract some of the following for extractInputs & extractOutputs
            if (controlsDict->hasProperty(typeKey))
            {
                String type = controlsDict->getProperty("type").toString().toStdString();

                if (type == "audio_track")
                {
                    std::shared_ptr<ModelComponentInfo> audioTrack =
                        std::make_shared<AudioTrackComponentInfo>(controlsDict);

                    newInputs.push_back(audioTrack);
                    tempComponentIDs.push_back(audioTrack->id);

                    DBG_AND_LOG("Model::extractInputs: Audio track input \"" + audioTrack->label
                                + "\" extracted.");
                }
                else if (type == "midi_track")
                {
                    std::shared_ptr<ModelComponentInfo> midiTrack =
                        std::make_shared<MidiTrackComponentInfo>(controlsDict);

                    newInputs.push_back(midiTrack);
                    tempComponentIDs.push_back(midiTrack->id);

                    DBG_AND_LOG("Model::extractInputs: MIDI track input \"" + midiTrack->label
                                + "\" extracted.");
                }
                else if (type == "text_box")
                {
                    std::shared_ptr<ModelComponentInfo> textControl =
                        std::make_shared<TextBoxComponentInfo>(controlsDict);

                    newControls.push_back(textControl);
                    tempComponentIDs.push_back(textControl->id);

                    DBG_AND_LOG("Model::extractInputs: Text control \"" + textControl->label
                                + "\" extracted.");
                }
                else if (type == "number_box")
                {
                    std::shared_ptr<ModelComponentInfo> numberControl =
                        std::make_shared<NumberBoxComponentInfo>(controlsDict);

                    newControls.push_back(numberControl);
                    tempComponentIDs.push_back(numberControl->id);

                    DBG_AND_LOG("Model::extractInputs: Number box control \"" + numberControl->label
                                + "\" extracted.");
                }
                else if (type == "toggle")
                {
                    std::shared_ptr<ModelComponentInfo> toggleControl =
                        std::make_shared<ToggleComponentInfo>(controlsDict);

                    newControls.push_back(toggleControl);
                    tempComponentIDs.push_back(toggleControl->id);

                    DBG_AND_LOG("Model::extractInputs: Toggle control \"" + toggleControl->label
                                + "\" extracted.");
                }
                else if (type == "slider")
                {
                    std::shared_ptr<ModelComponentInfo> sliderControl =
                        std::make_shared<SliderComponentInfo>(controlsDict);

                    newControls.push_back(sliderControl);
                    tempComponentIDs.push_back(sliderControl->id);

                    DBG_AND_LOG("Model::extractInputs: Slider control \"" + sliderControl->label
                                + "\" extracted.");
                }
                else if (type == "dropdown")
                {
                    std::shared_ptr<ModelComponentInfo> dropdownControl =
                        std::make_shared<ComboBoxComponentInfo>(controlsDict);

                    newControls.push_back(dropdownControl);
                    tempComponentIDs.push_back(dropdownControl->id);

                    DBG_AND_LOG("Model::extractInputs: Dropdown control \"" + dropdownControl->label
                                + "\" extracted.");
                }
                else
                {
                    return OpResult::fail(
                        ControlError { ControlError::Type::UnsupportedControl, type });
                }
            }
            else
            {
                return OpResult::fail(JsonError { JsonError::Type::MissingKey,
                                                  JSON::toString(var(controlsDict), true),
                                                  typeKey.toString() });
            }
        }

        return OpResult::ok();
    }

    OpResult extractOutputs(DynamicObject::Ptr& controls, ModelComponentInfoList& newOutputs)
    {
        static const Identifier outputsKey { "outputs" };

        Array<var>* outputComponents;

        OpResult result = getRequiredArrayProperty(controls, outputsKey, outputComponents);

        if (result.failed())
        {
            return result;
        }

        for (int i = 0; i < outputComponents->size(); i++)
        {
            var controlsVar = outputComponents->getReference(i);

            if (! controlsVar.isObject())
            {
                return OpResult::fail(
                    JsonError { JsonError::Type::NotADictionary, controlsVar.toString() });
            }

            DynamicObject* controlsDict = controlsVar.getDynamicObject();

            static const Identifier typeKey { "type" };

            if (controlsDict->hasProperty("type"))
            {
                String type = controlsDict->getProperty("type").toString().toStdString();

                if (type == "audio_track")
                {
                    std::shared_ptr<ModelComponentInfo> audioTrack =
                        std::make_shared<AudioTrackComponentInfo>(controlsDict);

                    newOutputs.push_back(audioTrack);

                    DBG_AND_LOG("Model::extractOutputs: Audio track output \"" + audioTrack->label
                                + "\" extracted.");
                }
                else if (type == "midi_track")
                {
                    std::shared_ptr<ModelComponentInfo> midiTrack =
                        std::make_shared<MidiTrackComponentInfo>(controlsDict);

                    newOutputs.push_back(midiTrack);

                    DBG_AND_LOG("Model::extractOutputs: MIDI track output \"" + midiTrack->label
                                + "\" extracted.");
                }
                else if (type == "json")
                {
                    // Labels are handled separately

                    DBG_AND_LOG("Model::extractOutputs: JSON (labels) output detected.");
                }
                else
                {
                    return OpResult::fail(
                        ControlError { ControlError::Type::UnsupportedControl, type });
                }
            }
            else
            {
                return OpResult::fail(JsonError { JsonError::Type::MissingKey,
                                                  JSON::toString(var(controlsDict), true),
                                                  typeKey.toString() });
            }
        }

        return OpResult::ok();
    }

    std::shared_ptr<ModelComponentInfo> getComponentInfo(const Uuid& id) const
    {
        for (const auto& controlComponent : controlComponents)
        {
            if (id == controlComponent->id)
            {
                return controlComponent;
            }
        }

        for (const auto& inputTrackComponent : inputTrackComponents)
        {
            if (id == inputTrackComponent->id)
            {
                return inputTrackComponent;
            }
        }

        for (const auto& outputTrackComponent : outputTrackComponents)
        {
            if (id == outputTrackComponent->id)
            {
                return outputTrackComponent;
            }
        }

        return nullptr;
    }

    ModelStatus status;

    std::unique_ptr<Client> client;

    String loadedPath;
    String openablePath;

    ModelMetadata metadata;

    std::vector<Uuid> tempComponentIDs;
    std::vector<Uuid> orderedInputComponentIDs;

    ModelComponentInfoList controlComponents;

    ModelComponentInfoList inputTrackComponents;
    ModelComponentInfoList outputTrackComponents;

    SharedResourcePointer<StatusMessage> statusMessage;
};
