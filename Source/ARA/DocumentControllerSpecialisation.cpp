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


// The constructor. It is taking entry and instance as parameters and feeds them directly to the base class constructor.
HARPDocumentControllerSpecialisation::
            HARPDocumentControllerSpecialisation(const ARA::PlugIn::PlugInEntry* entry,
                                         const ARA::ARADocumentControllerHostInstance* instance)
            : ARADocumentControllerSpecialisation(entry, instance),
            // juce::Thread("executeProcessingThread")
            juce::ThreadWithProgressWindow ("Processing",
                              true,
                              true,
                              10000,
                              "cancel (don't click me)"
                              )
             {
              setStatusMessage ("Processing...");
}


void HARPDocumentControllerSpecialisation::executeLoad(const map<string, any> &params) {
    // get the modelPath, pass it to the model
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
  }

void HARPDocumentControllerSpecialisation::run() {
  // This function is called when the thread is started
  // Show the processing modal window
  // processingWindow = std::make_unique<juce::AlertWindow>(
  //     "Processing",
  //     "Processing...",
  //     juce::AlertWindow::NoIcon);

  // processingWindow_->enterModalState();

  // Each playbackRenderer has access to its own playbackRegions
  setProgress(0.0);
  int counter = 0;
  for (auto& playbackRenderer : playbackRenderers) {
    playbackRenderer->executeProcess(mModel);
    counter++;
    setProgress((double) counter / (double) playbackRenderers.size());
    double pr = (double) counter / (double) playbackRenderers.size();
    DBG("Progress: " << pr);
  }
  

  // Dismiss the modal window when processing is complete
  // processingWindow->exitModalState();
}

void HARPDocumentControllerSpecialisation::executeProcess(std::shared_ptr<WebWave2Wave> model) {
  // wait untill thread has stopped running
  if (!isThreadRunning()) {
    // start the thread
    if (model == nullptr){
      DBG("unhandled exception: model is null. we should probably open an error window here.");
      return;
    }

    mModel = model;
    launchThread();
  }
}

void 	HARPDocumentControllerSpecialisation::threadComplete (bool userPressedCancel) {
  // if the user didn't press cancel, then the thread has finished normally
  // and we can trigger the repainting of the playbackRegions to update the
  // processed waveform thumbnails
  if (!userPressedCancel) {
    editorView->triggerRepaint();
  }
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

ARAPlaybackRenderer *HARPDocumentControllerSpecialisation::
    doCreatePlaybackRenderer() noexcept {
  PlaybackRenderer* newPlaybackRenderer = new PlaybackRenderer(getDocumentController(), *this);
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
