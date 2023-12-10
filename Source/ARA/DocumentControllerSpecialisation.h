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

#pragma once

#include "EditorRenderer.h"
// #include "PlaybackRenderer.h"
#include "EditorView.h"
#include "../DeepLearning/WebModel.h"
#include "juce_core/juce_core.h"


#include "../Util/PreviewState.h"

// Forward declaration of the child class
class PlaybackRenderer;

class CustomThreadPoolJob : public ThreadPoolJob {
  public:
    CustomThreadPoolJob(std::function<void()> jobFunction)
        : ThreadPoolJob("CustomThreadPoolJob"), jobFunction(jobFunction)
    {}

    JobStatus runJob() override {
      if (jobFunction) {
          jobFunction();
          return jobHasFinished;
      }
      else {
          return jobHasFinished; // or some other appropriate status
      }
    }

  private:
    std::function<void()> jobFunction;
};

// I don't like this
class JobProcessorThread : public Thread {
  public:
    JobProcessorThread(const std::vector<CustomThreadPoolJob*>& jobs,
                        int& jobsFinished,
                        int& totalJobs,
                        ChangeBroadcaster& processBroadcaster
                    )
        : Thread("JobProcessorThread"),
        customJobs(jobs),
        jobsFinished(jobsFinished),
        totalJobs(totalJobs),
        processBroadcaster(processBroadcaster)
    {}

    void run() override {
      for (auto& customJob : customJobs) {
          threadPool.addJob(customJob, true); // The pool will take ownership and delete the job when finished
      }

      // Wait for all jobs to finish
      for (auto& customJob : customJobs) {
          threadPool.waitForJobToFinish(customJob, -1); // -1 for no timeout
      }

      // This will run after all jobs are done
      // if (jobsFinished == totalJobs) {
      processBroadcaster.sendChangeMessage();
      DBG("Jobs finished");
      // }
    }

  private:
    const std::vector<CustomThreadPoolJob*>& customJobs;
    int& jobsFinished;
    int& totalJobs;
    juce::ThreadPool threadPool {3};
    ChangeBroadcaster& processBroadcaster;
};

/**
 * @class HARPDocumentControllerSpecialisation
 * @brief Specialises ARA's document controller, with added functionality for
 * audio modifications, playback rendering, and editor rendering.
 */
class HARPDocumentControllerSpecialisation
    : public ARADocumentControllerSpecialisation,
      private ProcessingLockInterface {
public:
  /**
   * @brief Constructor.
   * Uses ARA's document controller specialisation's constructor.
   */
  HARPDocumentControllerSpecialisation(const ARA::PlugIn::PlugInEntry* entry,
                                         const ARA::ARADocumentControllerHostInstance* instance) ;

  PreviewState previewState; ///< Preview state.

  std::shared_ptr<WebWave2Wave> getModel() { return mModel; }
  void executeLoad(const map<string, any> &params);
  void executeProcess(std::shared_ptr<WebWave2Wave> model);

  void cleanDeletedPlaybackRenderers(PlaybackRenderer* playbackRendererToDelete);

  void addLoadListener(ChangeListener* listener);
  void removeLoadListener(ChangeListener* listener);
  void addProcessListener(ChangeListener* listener);
  void removeProcessListener(ChangeListener* listener);
  bool isLoadBroadcaster(ChangeBroadcaster* broadcaster);
  bool isProcessBroadcaster(ChangeBroadcaster* broadcaster);

protected:
  void willBeginEditing(
      ARADocument *) override; ///< Called when beginning to edit a document.
  void didEndEditing(
      ARADocument *) override; ///< Called when editing a document ends.

  /**
   * @brief Creates an audio modification.
   * @return A new AudioModification instance.
   */
  ARAAudioModification *doCreateAudioModification(
      ARAAudioSource *audioSource, ARA::ARAAudioModificationHostRef hostRef,
      const ARAAudioModification *optionalModificationToClone) noexcept
      override;

  /**
   * @brief Creates a playback renderer.
   * @return A new PlaybackRenderer instance.
   */
  ARAPlaybackRenderer *doCreatePlaybackRenderer() noexcept override;

  // /**
  //  * @brief Creates an editor renderer.
  //  * @return A new EditorRenderer instance.
  //  */
  // EditorRenderer *doCreateEditorRenderer() noexcept override;

  /**
   * @brief Creates an editor view.
   * @return A new EditorView instance.
   */
  EditorView *doCreateEditorView() noexcept override;

  bool
  doRestoreObjectsFromStream(ARAInputStream &input,
                             const ARARestoreObjectsFilter *filter) noexcept
      override; ///< Restores objects from a stream.
  bool doStoreObjectsToStream(ARAOutputStream &output,
                              const ARAStoreObjectsFilter *filter) noexcept
      override; ///< Stores objects to a stream.

private:


  ScopedTryReadLock getProcessingLock() override; ///< Gets the processing lock.

  ReadWriteLock processBlockLock; ///< Lock for processing blocks.

  // We need the DocController to have access to the EditorView
  // so that we can update the view when the model is loaded
  // (another option is to use a listener pattern)
  EditorView *editorView {nullptr}; ///< Editor view.
  // PlaybackRenderer *playbackRenderer {nullptr}; ///< Playback renderer.
  EditorRenderer *editorRenderer {nullptr}; ///< Editor renderer.

  // store a copy of the model
  std::shared_ptr<WebWave2Wave> mModel {new WebWave2Wave()}; ///< Model for audio processing.

  // In contrast to EditorView and EditorRenderer which are unique for the plugin
  // there are multiple playbackRenderers (one for each playbackRegion)
  std::vector<PlaybackRenderer*> playbackRenderers;
  std::unique_ptr<juce::AlertWindow> processingWindow;

  // This one is used for Loading the models
  // The thread pull for Processing lives inside the JobProcessorThread
  juce::ThreadPool threadPool {1};

  std::vector<CustomThreadPoolJob*> customJobs;
  // ChangeBroadcaster processBroadcaster;
  int jobsFinished = 0;
  int totalJobs = 0;
  juce::ChangeBroadcaster loadBroadcaster;
  juce::ChangeBroadcaster processBroadcaster;
  JobProcessorThread jobProcessorThread;

};

