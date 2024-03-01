#include "juce_core/juce_core.h"
#include "juce_events/juce_events.h"
using namespace juce;

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
          return jobHasFinished;
      }
    }

  private:
    std::function<void()> jobFunction;
};

class JobProcessorThread : public Thread {
  public:
    JobProcessorThread(const std::vector<CustomThreadPoolJob*>& jobs,
                        int& jobsFinished,
                        int& totalJobs,
                        ChangeBroadcaster& broadcaster
                    )
        : Thread("JobProcessorThread"),
        customJobs(jobs),
        jobsFinished(jobsFinished),
        totalJobs(totalJobs),
        processBroadcaster(broadcaster)
    {}

    ~JobProcessorThread() override {
      // threadPool.removeAllJobs(true, 1000);
    }

    // void initiateJobs() {}
    void run() override {
      while (!threadShouldExit()) {
        // Wait for a signal to execute a task
        signalEvent.wait(-1);

        // Check if the thread should exit before executing the task
        if (threadShouldExit()){
          DBG("Thread should exit");
          break;
        }
        // Execute your task here
        executeTask();
      }
    }

    void signalTask() {
      // Send a signal to wake up the thread and execute a task
      signalEvent.signal();
    }

  private:

    void executeTask() {
      for (auto& customJob : customJobs) {
            threadPool.addJob(customJob, true); // The pool will take ownership and delete the job when finished
            // customJob->runJob();
        }

        // Wait for all jobs to finish
        for (auto& customJob : customJobs) {
            threadPool.waitForJobToFinish(customJob, -1); // -1 for no timeout
        }

        // This will run after all jobs are done
        // if (jobsFinished == totalJobs) {
        processBroadcaster.sendChangeMessage();
        // }
    }

    const std::vector<CustomThreadPoolJob*>& customJobs;
    int& jobsFinished;
    int& totalJobs;
    // ThreadPool for processing jobs (not loading)
    ThreadPool threadPool {10};
    ChangeBroadcaster& processBroadcaster;
    WaitableEvent signalEvent;
};