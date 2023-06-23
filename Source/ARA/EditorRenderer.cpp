/**
  * @file
  * @brief This file is part of the JUCE examples.
  * 
  * Copyright (c) 2022 - Raw Material Software Limited
  * The code included in this file is provided under the terms of the ISC license
  * http://www.isc.org/downloads/software-support-policy/isc-license. Permission
  * To use, copy, modify, and/or distribute this software for any purpose with or
  * without fee is hereby granted provided that the above copyright notice and
  * this permission notice appear in all copies.
  * 
  * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
  * WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
  * PURPOSE, ARE DISCLAIMED.
  * 
  * @brief Implementation of the ARA Editor Renderer.
  * The ARA Editor Renderer is used to handle audio feedback from the plugin.
  * This allows us to playback samples from the host without the host in playback mode.
  * Editor Renderer will handle any audio rendering that is done soley in the plugin.
  * @author JUCE, aldo aguilar, hugo flores garcia
  */

/**
 * @file EditorRenderer.cpp
 * @brief Implements the EditorRenderer class.
 */

#include "EditorRenderer.h"

std::optional<Range<int64>> readPlaybackRangeIntoBuffer (Range<double> playbackRange,
                                                          const ARAPlaybackRegion* playbackRegion,
                                                          AudioBuffer<float>& buffer,
                                                          const std::function<AudioFormatReader* (ARAAudioSource*)>& getReader) {
    const auto rangeInAudioModificationTime = playbackRange - playbackRegion->getStartInPlaybackTime()
                                                            + playbackRegion->getStartInAudioModificationTime();

    const auto audioModification = playbackRegion->getAudioModification<AudioModification>();
    const auto audioSource = audioModification->getAudioSource();
    const auto audioModificationSampleRate = audioSource->getSampleRate();

    const Range<int64_t> sampleRangeInAudioModification {
        ARA::roundSamplePosition (rangeInAudioModificationTime.getStart() * audioModificationSampleRate),
        ARA::roundSamplePosition (rangeInAudioModificationTime.getEnd() * audioModificationSampleRate) - 1
    };

    const auto inputOffset = jlimit ((int64_t) 0, audioSource->getSampleCount(), sampleRangeInAudioModification.getStart());

    // With the output offset it can always be said of the output buffer, that the zeroth element
    // corresponds to beginning of the playbackRange.
    const auto outputOffset = std::max (-sampleRangeInAudioModification.getStart(), (int64_t) 0);

    /* TODO: Handle different AudioSource and playback sample rates.

       The conversion should be done inside a specialized AudioFormatReader so that we could use
       playbackSampleRate everywhere in this function and we could still read `readLength` number of samples
       from the source.

       The current implementation will be incorrect when sampling rates differ.
    */
    const auto readLength = [&]
    {
        const auto sourceReadLength =
            std::min (sampleRangeInAudioModification.getEnd(), audioSource->getSampleCount()) - inputOffset;

        const auto outputReadLength =
            std::min (outputOffset + sourceReadLength, (int64_t) buffer.getNumSamples()) - outputOffset;

        return std::min (sourceReadLength, outputReadLength);
    }();

    if (readLength == 0)
        return Range<int64>();

    auto* reader = getReader (audioSource);

    if (reader != nullptr && reader->read (&buffer, (int) outputOffset, (int) readLength, inputOffset, true, true))
    {
        if (audioModification->isDimmed())
            buffer.applyGain ((int) outputOffset, (int) readLength, 0.25f);

        return Range<int64>::withStartAndLength (outputOffset, readLength);
    }

    return {};
}

EditorRenderer::EditorRenderer (ARA::PlugIn::DocumentController* documentController,
                                const PreviewState* previewStateIn,
                                ProcessingLockInterface& lockInterfaceIn)
    : ARAEditorRenderer (documentController),
      lockInterface (lockInterfaceIn),
      previewState (previewStateIn),
      asyncConfigCallback { [this] { configure(); } }
{
    jassert (previewState != nullptr);
}

EditorRenderer::~EditorRenderer()
{
    for (const auto& rs : regionSequences)
        rs->removeListener (this);
}

void EditorRenderer::didAddPlaybackRegionToRegionSequence (ARARegionSequence*, ARAPlaybackRegion*)
{
    asyncConfigCallback.startConfigure();
}

void EditorRenderer::didAddRegionSequence (ARA::PlugIn::RegionSequence* rs) noexcept
{
    auto* sequence = static_cast<ARARegionSequence*> (rs);
    sequence->addListener (this);
    regionSequences.insert (sequence);
    asyncConfigCallback.startConfigure();
}

void EditorRenderer::didAddPlaybackRegion (ARA::PlugIn::PlaybackRegion*) noexcept
{
    asyncConfigCallback.startConfigure();
}

void EditorRenderer::executeProcess(std::map<std::string, std::any> &params)
{
    DBG("EditorRenderer::executeProcess executing process");

    auto myCallback = [&params](ARAPlaybackRegion* playbackRegion) -> bool {
        auto audioModification = playbackRegion->getAudioModification<AudioModification>();
        std::cout << "EditorRenderer::processing playbackRegion " << audioModification->getSourceName() << std::endl;
        audioModification->process(params);
        return true;
    };

    forEachPlaybackRegion(myCallback);
}

template <typename Callback>
void EditorRenderer::forEachPlaybackRegion (Callback&& cb)
{
    for (const auto& playbackRegion : getPlaybackRegions())
        if (! cb (playbackRegion))
            return;

    for (const auto& regionSequence : getRegionSequences())
        for (const auto& playbackRegion : regionSequence->getPlaybackRegions())
            if (! cb (playbackRegion))
                return;
}

void EditorRenderer::prepareToPlay (double sampleRateIn,
                                    int maximumExpectedSamplesPerBlock,
                                    int numChannels,
                                    AudioProcessor::ProcessingPrecision,
                                    AlwaysNonRealtime alwaysNonRealtime)
{
    sampleRate = sampleRateIn;
    previewBuffer = std::make_unique<AudioBuffer<float>> (numChannels, (int) (4 * sampleRateIn));

    ignoreUnused (maximumExpectedSamplesPerBlock, alwaysNonRealtime);
}

void EditorRenderer::releaseResources()
{
    audioSourceReaders.clear();
}

void EditorRenderer::reset()
{
    previewBuffer->clear();
}

bool EditorRenderer::processBlock (AudioBuffer<float>& buffer,
                                   AudioProcessor::Realtime realtime,
                                   const AudioPlayHead::PositionInfo& positionInfo) noexcept
{

    ignoreUnused (realtime);

    const auto lock = lockInterface.getProcessingLock();

    if (! lock.isLocked())
        return true;

    return asyncConfigCallback.withLock ([&] (bool locked)
    {
        if (! locked)
            return true;

        const auto fadeOutIfNecessary = [this, &buffer]
        {
            if (std::exchange (wasPreviewing, false))
            {
                previewLooper.writeInto (buffer);
                const auto fadeOutStart = std::max (0, buffer.getNumSamples() - 50);
                buffer.applyGainRamp (fadeOutStart, buffer.getNumSamples() - fadeOutStart, 1.0f, 0.0f);
            }
        };

        if (positionInfo.getIsPlaying())
        {
            fadeOutIfNecessary();
            return true;
        }

        if (const auto previewedRegion = previewState->previewedRegion.load())
        {
            const auto regionIsAssignedToEditor = [&]()
            {
                bool regionIsAssigned = false;

                forEachPlaybackRegion ([&previewedRegion, &regionIsAssigned] (const auto& region)
                {
                    if (region == previewedRegion)
                    {
                        regionIsAssigned = true;
                        return false;
                    }

                    return true;
                });

                return regionIsAssigned;
            }();

            if (regionIsAssignedToEditor)
            {
                const auto previewTime = previewState->previewTime.load();
                const auto previewDimmed = previewedRegion->getAudioModification<AudioModification>()->isDimmed();

                if (lastPreviewTime != previewTime
                    || lastPlaybackRegion != previewedRegion
                    || lastPreviewDimmed != previewDimmed)
                {
                    Range<double> previewRangeInPlaybackTime { previewTime - 0.5, previewTime + 0.5 };

                    previewBuffer->clear();
                    const auto rangeInOutput = readPlaybackRangeIntoBuffer (previewRangeInPlaybackTime,
                                                                            previewedRegion,
                                                                            *previewBuffer,
                                                                            [this] (auto* source) -> auto*
                                                                            {
                                                                                const auto iter = audioSourceReaders.find (source);
                                                                                return iter != audioSourceReaders.end() ? iter->second.get() : nullptr;
                                                                            });

                    if (rangeInOutput)
                    {
                        lastPreviewTime = previewTime;
                        lastPlaybackRegion = previewedRegion;
                        lastPreviewDimmed = previewDimmed;
                        previewLooper = Looper (previewBuffer.get(), *rangeInOutput);
                    }
                }
                else
                {
                    previewLooper.writeInto (buffer);

                    if (! std::exchange (wasPreviewing, true))
                    {
                        const auto fadeInLength = std::min (50, buffer.getNumSamples());
                        buffer.applyGainRamp (0, fadeInLength, 0.0f, 1.0f);
                    }
                }
            }
        }
        else
        {
            fadeOutIfNecessary();
        }

        return true;
    });
}

void EditorRenderer::configure()
{
    forEachPlaybackRegion ([this, maximumExpectedSamplesPerBlock = 1000] (const auto& playbackRegion)
    {
        const auto audioSource = playbackRegion->getAudioModification()->getAudioSource();

        if (audioSourceReaders.find (audioSource) == audioSourceReaders.end())
        {
            audioSourceReaders[audioSource] = std::make_unique<BufferingAudioReader> (
                    new ARAAudioSourceReader (playbackRegion->getAudioModification()->getAudioSource()),
                    *timeSliceThread,
                    std::max (4 * maximumExpectedSamplesPerBlock, (int) sampleRate));
        }

        return true;
    });
}
