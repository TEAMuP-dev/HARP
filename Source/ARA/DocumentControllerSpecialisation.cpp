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
 * @brief Implementation of the ARA document controller.
 * This controller is needed for any ARA plugin and handles functions for
 * editing and playback of audio, analysis of content from the host, and it
 * maintains the ARA model graph. More information can be found on the offical
 * JUCE documentation:
 * https://docs.juce.com/master/classARADocumentControllerSpecialisation.html#details
 * @author JUCE, aldo aguilar, hugo flores garcia
 */

#include "DocumentControllerSpecialisation.h"

void TensorJuceDocumentControllerSpecialisation::willBeginEditing(
    ARADocument *) {
  processBlockLock.enterWrite();
}

void TensorJuceDocumentControllerSpecialisation::didEndEditing(ARADocument *) {
  processBlockLock.exitWrite();
}

ARAAudioModification *
TensorJuceDocumentControllerSpecialisation::doCreateAudioModification(
    ARAAudioSource *audioSource, ARA::ARAAudioModificationHostRef hostRef,
    const ARAAudioModification *optionalModificationToClone) noexcept {
    

  return new AudioModification(
      audioSource, hostRef,
      static_cast<const AudioModification *>(optionalModificationToClone), 
      mModel
    );
}

ARAPlaybackRenderer *TensorJuceDocumentControllerSpecialisation::
    doCreatePlaybackRenderer() noexcept {
  return new PlaybackRenderer(getDocumentController(), *this);
}

EditorRenderer *
TensorJuceDocumentControllerSpecialisation::doCreateEditorRenderer() noexcept {
  return new EditorRenderer(getDocumentController(), &previewState, *this);
}

bool TensorJuceDocumentControllerSpecialisation::doRestoreObjectsFromStream(
    ARAInputStream &input, const ARARestoreObjectsFilter *filter) noexcept {
  // Start reading data from the archive, starting with the number of audio
  // modifications in the archive
  const auto numAudioModifications = input.readInt64();

  // Loop over stored audio modification data
  for (int64 i = 0; i < numAudioModifications; ++i) {
    const auto progressVal = (float)i / (float)numAudioModifications;
    getDocumentController()
        ->getHostArchivingController()
        ->notifyDocumentUnarchivingProgress(progressVal);

    // Read audio modification persistent ID and analysis result from archive
    const String persistentID = input.readString();
    const bool dimmed = input.readBool();

    // Find audio modification to restore the state to (drop state if not to be
    // loaded)
    auto audioModification =
        filter->getAudioModificationToRestoreStateWithID<AudioModification>(
            persistentID.getCharPointer());

    if (audioModification == nullptr)
      continue;

    const bool dimChanged = (dimmed != audioModification->isDimmed());
    audioModification->setDimmed(dimmed);

    // If the dim state changed, send a sample content change notification
    // without notifying the host
    if (dimChanged) {
      audioModification->notifyContentChanged(
          ARAContentUpdateScopes::samplesAreAffected(), false);

      for (auto playbackRegion : audioModification->getPlaybackRegions())
        playbackRegion->notifyContentChanged(
            ARAContentUpdateScopes::samplesAreAffected(), false);
    }
  }

  getDocumentController()
      ->getHostArchivingController()
      ->notifyDocumentUnarchivingProgress(1.0f);

  return !input.failed();
}

bool TensorJuceDocumentControllerSpecialisation::doStoreObjectsToStream(
    ARAOutputStream &output, const ARAStoreObjectsFilter *filter) noexcept {
  // This example implementation only deals with audio modification states
  const auto &audioModificationsToPersist{
      filter->getAudioModificationsToStore<AudioModification>()};

  const auto reportProgress =
      [archivingController =
           getDocumentController()->getHostArchivingController()](float p) {
        archivingController->notifyDocumentArchivingProgress(p);
      };

  const ScopeGuard scope{[&reportProgress] { reportProgress(1.0f); }};

  // Write the number of audio modifications we are persisting
  const auto numAudioModifications = audioModificationsToPersist.size();

  if (!output.writeInt64((int64)numAudioModifications))
    return false;

  // For each audio modification to persist, persist its ID followed by whether
  // it's dimmed
  for (size_t i = 0; i < numAudioModifications; ++i) {
    // Write persistent ID and dim state
    if (!output.writeString(audioModificationsToPersist[i]->getPersistentID()))
      return false;

    if (!output.writeBool(audioModificationsToPersist[i]->isDimmed()))
      return false;

    const auto progressVal = (float)i / (float)numAudioModifications;
    reportProgress(progressVal);
  }

  return true;
}

ScopedTryReadLock
TensorJuceDocumentControllerSpecialisation::getProcessingLock() {
  return ScopedTryReadLock{processBlockLock};
}
