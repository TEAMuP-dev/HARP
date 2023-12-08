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
 * @author JUCE, aldo aguilar, hugo flores garcia, xribene
 */

#include <juce_gui_basics/juce_gui_basics.h>  // Include this if it's not already included
#include "DocumentControllerSpecialisation.h"
#include "PlaybackRenderer.h"

// The constructor. It is taking entry and instance as parameters and feeds them directly to the base class constructor.
HARPDocumentControllerSpecialisation::
            HARPDocumentControllerSpecialisation(const ARA::PlugIn::PlugInEntry* entry,
                                         const ARA::ARADocumentControllerHostInstance* instance)
            : ARADocumentControllerSpecialisation(entry, instance) {
}


void HARPDocumentControllerSpecialisation::cleanDeletedPlaybackRenderers(PlaybackRenderer* playbackRendererToDelete){
    auto it = std::remove(playbackRenderers.begin(), playbackRenderers.end(), playbackRendererToDelete);
    playbackRenderers.erase(it, playbackRenderers.end());
    DBG("playbackRenderers.size() after: " << playbackRenderers.size());
}

void HARPDocumentControllerSpecialisation::executeLoad(const map<string, any> &params) {
    // TODO: should we cancel all other jobs? 
    // how do we deal with a process job that is currently running? 

    // get the modelPath, pass it to the model
    threadPool.addJob([this, params] {
      DBG("HARPDocumentControllerSpecialisation::executeLoad");
      try {
          mModel->load(params);
      } catch (const std::runtime_error& e) {
          juce::AlertWindow::showMessageBoxAsync(
              juce::AlertWindow::WarningIcon,
              "Loading Error",
              juce::String("An error occurred while loading the WebModel: ") + e.what()
          );
      }
      DBG("HARPDocumentControllerSpecialisation::executeLoad done");
      loadBroadcaster.sendChangeMessage();
    });

  }

void HARPDocumentControllerSpecialisation::executeProcess(std::shared_ptr<WebWave2Wave> model) {
  // TODO: need to only be able to do this if we don't have any other jobs in the threadpool right? 
  if (model == nullptr){
    DBG("unhandled exception: model is null. we should probably open an error window here.");
    return;
  }

  // print how many jobs are currently in the threadpool
  DBG("threadPool.getNumJobs: " << threadPool.getNumJobs());

  mModel = model;

  // start the thread
  threadPool.addJob([this] {
    int counter = 0;
    for (auto& playbackRenderer : playbackRenderers) {
      playbackRenderer->executeProcess(mModel);

      counter++;
      DBG("Processing region: " << counter);
    }
    processBroadcaster.sendChangeMessage();
  });
}

void HARPDocumentControllerSpecialisation::willBeginEditing(
    ARADocument *) {
  processBlockLock.enterWrite();
}

void HARPDocumentControllerSpecialisation::didEndEditing(ARADocument *) {
  processBlockLock.exitWrite();
}

ARAAudioModification *
HARPDocumentControllerSpecialisation::doCreateAudioModification(
    ARAAudioSource *audioSource, ARA::ARAAudioModificationHostRef hostRef,
    const ARAAudioModification *optionalModificationToClone) noexcept {
    

  return new AudioModification(
      audioSource, hostRef,
      static_cast<const AudioModification *>(optionalModificationToClone)
    );
}

ARAPlaybackRegion*
HARPDocumentControllerSpecialisation::doCreatePlaybackRegion (
    ARAAudioModification* modification,
    ARA::ARAPlaybackRegionHostRef hostRef) noexcept {
    PlaybackRegion* newPlaybackRegion = new PlaybackRegion(modification, hostRef);
    ARAPlaybackRegion* newARAPlaybackRegion = static_cast<ARAPlaybackRegion*>(newPlaybackRegion);
  return newARAPlaybackRegion;
}

// ARAPlaybackRegion*
// HARPDocumentControllerSpecialisation::doCreatePlaybackRegion (
//     ARAAudioModification* modification,
//     ARA::ARAPlaybackRegionHostRef hostRef) noexcept {
//   PlaybackRegion* newPlaybackRegion = new PlaybackRegion(modification, hostRef);
//   // dynamic cast PlaybackRegion to ARAPlaybackRegion
//     ARAPlaybackRegion* newARAPlaybackRegion = static_cast<ARAPlaybackRegion*>(newPlaybackRegion);
//     // ARAPlaybackRegion* newARAPlaybackRegion = new ARAPlaybackRegion(modification, hostRef);
//   return newARAPlaybackRegion;
// }

ARAPlaybackRenderer *HARPDocumentControllerSpecialisation::
    doCreatePlaybackRenderer() noexcept {
  PlaybackRenderer* newPlaybackRenderer = new PlaybackRenderer(getDocumentController(), *this, *this);
  playbackRenderers.push_back(newPlaybackRenderer);
  return newPlaybackRenderer;
}

// TODO : why not use ARAEditorRenderer like above ? (ARAPlaybackRenderer)
// EditorRenderer *
// HARPDocumentControllerSpecialisation::doCreateEditorRenderer() noexcept {
//   // return new EditorRenderer(getDocumentController(), &previewState, *this);
//   EditorRenderer* newEditorRenderer = new EditorRenderer(getDocumentController(), &previewState, *this);
//   editorRenderer = newEditorRenderer;
//   return newEditorRenderer;
// }

// Use ARAEditorView instead of EditorView because DocumentView expects just that. 
// TODO : change the type in DocumentView
EditorView *
HARPDocumentControllerSpecialisation::doCreateEditorView() noexcept {
  EditorView* newEditorView = new EditorView(getDocumentController());
  editorView = newEditorView;//dynamic_cast<EditorView*>(newEditorView);
  editorView->setModel(mModel);
  return newEditorView;
}

bool HARPDocumentControllerSpecialisation::doRestoreObjectsFromStream(
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

    // Find audio modification to restore the state to (drop state if not to be
    // loaded)
    auto audioModification =
        filter->getAudioModificationToRestoreStateWithID<AudioModification>(
            persistentID.getCharPointer());

    if (audioModification == nullptr)
      continue;

    // If the dim state changed, send a sample content change notification
    // without notifying the host
    if (audioModification->getIsModified()) {
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


bool HARPDocumentControllerSpecialisation::doStoreObjectsToStream(
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
    const auto progressVal = (float)i / (float)numAudioModifications;
    reportProgress(progressVal);
  }

  return true;
}

ScopedTryReadLock
HARPDocumentControllerSpecialisation::getProcessingLock() {
  return ScopedTryReadLock{processBlockLock};
}
