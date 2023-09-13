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

#include "DocumentControllerSpecialisation.h"

// The constructor. It is taking entry and instance as parameters and feeds them directly to the base class constructor.
TensorJuceDocumentControllerSpecialisation::
            TensorJuceDocumentControllerSpecialisation(const ARA::PlugIn::PlugInEntry* entry,
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

void TensorJuceDocumentControllerSpecialisation::printModelPath(std::string path) {
  std::cout << "Model path: " << path << std::endl;
  DBG("Model path: " << path);
}

void TensorJuceDocumentControllerSpecialisation::executeLoad(const std::string &modelPath) {
    // get the modelPath, pass it to the model
    DBG("TensorJuceDocumentControllerSpecialisation::executeLoad");
    mModel->load(modelPath);
    DBG("TensorJuceDocumentControllerSpecialisation::executeLoad done");
  }

void TensorJuceDocumentControllerSpecialisation::run() {
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
    playbackRenderer->executeProcess(processingParams);
    counter++;
    setProgress((double) counter / (double) playbackRenderers.size());
    double pr = (double) counter / (double) playbackRenderers.size();
    DBG("Progress: " << pr);
  }
  
  // Alternatively we could use Editor renderer 
  // which has access to all playbackRegions and regionSequences.
  // editorRenderer->executeProcess(params);

  // Dismiss the modal window when processing is complete
  // processingWindow->exitModalState();
}
void TensorJuceDocumentControllerSpecialisation::executeProcess(std::map<std::string, std::any> &params) {
  // wait untill thread has stopped running
  if (!isThreadRunning()) {
    // start the thread
    processingParams = params;
    // startThread();
    launchThread();
  }
}

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
  PlaybackRenderer* newPlaybackRenderer = new PlaybackRenderer(getDocumentController(), *this);
  playbackRenderers.push_back(newPlaybackRenderer);
  return newPlaybackRenderer;
}

// TODO : why not use ARAEditorRenderer like above ? (ARAPlaybackRenderer)
EditorRenderer *
TensorJuceDocumentControllerSpecialisation::doCreateEditorRenderer() noexcept {
  // return new EditorRenderer(getDocumentController(), &previewState, *this);
  EditorRenderer* newEditorRenderer = new EditorRenderer(getDocumentController(), &previewState, *this);
  editorRenderer = newEditorRenderer;
  return newEditorRenderer;
}

// Use ARAEditorView instead of EditorView because DocumentView expects just that. 
// TODO : change the type in DocumentView
EditorView *
TensorJuceDocumentControllerSpecialisation::doCreateEditorView() noexcept {
  EditorView* newEditorView = new EditorView(getDocumentController());
  editorView = newEditorView;//dynamic_cast<EditorView*>(newEditorView);
  mModel->addListener(editorView);
  return newEditorView;
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
