#include "juce_core/juce_core.h"
#include "juce_events/juce_events.h"
using namespace juce;

class CustomThreadPoolJob : public ThreadPoolJob
{
public:
    CustomThreadPoolJob(std::function<void(String)> _jobFunction, String jobID)
        : ThreadPoolJob(jobID), jobFunction(_jobFunction)
    {
    }

    ~CustomThreadPoolJob() override { DBG("Job Stopped"); }

    JobStatus runJob() override
    {
        if (jobFunction)
        {
            jobFunction(getJobName());
            return jobHasFinished;
        }
        else
        {
            return jobHasFinished;
        }
    }

private:
    std::function<void(String)> jobFunction;
};

/* This Class is no longer in use!!!!!
class JobProcessorThread : public Thread
{
public:
  JobProcessorThread( //const
		       std::vector<CustomThreadPoolJob*>& jobs,
                       int& _jobsFinished,
                       int& _totalJobs,
                       ChangeBroadcaster& broadcaster)
        : Thread("JobProcessorThread"),
          customJobs(jobs),
          jobsFinished(jobsFinished),
          totalJobs(totalJobs),
          processBroadcaster(broadcaster)
    {
    }

    ~JobProcessorThread() override
    {
        // threadPool.removeAllJobs(true, 1000);
    }

    // void initiateJobs() {}
    void run() override
    {
        while (! threadShouldExit())
        {
            // Wait for a signal to execute a task
	  DBG("WAITING FOR SIGNAL...");
            signalEvent.wait(-1);

            // Check if the thread should exit before executing the task
            if (threadShouldExit())
            {
                DBG("Thread should exit");
                break;
            }
            // Execute your task here
            executeTask();
        }
    }

    void signalTask()
    {
        // Send a signal to wake up the thread and execute a task
        signalEvent.signal();
    }

  void cancelAllJobs() {
    DBG("CANCEL ALL JOBS RECEIVED");
    // bool result = threadPool.removeAllJobs(true, -1);
    // DBG((result?"STOPPED":"FAIL TO STOP"));
    for (auto& customJob : customJobs) {
      customJob -> signalJobShouldExit();
    }
  }

private:
    void executeTask()
    {
      DBG("EXECUTE TASK");
      // threadPool.addJob(customJobs.front());
      DBG("JOBS ADDED");
      
        for (auto& customJob : customJobs)
        {
            threadPool.addJob(
                customJob, true); // The pool will take ownership and delete the job when finished
            // customJob->runJob();
        }

        // Wait for all jobs to finish
        for (auto& customJob : customJobs)
        {
            threadPool.waitForJobToFinish(customJob, -1); // -1 for no timeout
        }
	DBG("ALL JOB FINISHED");
	customJobs.clear();

        // This will run after all jobs are done
        // if (jobsFinished == totalJobs) {
        // processBroadcaster.sendChangeMessage();
        // }
    }

  // const
    std::vector<CustomThreadPoolJob*>& customJobs;

    // HUGO: these are unused. What are they for?
    int& jobsFinished;
    int& totalJobs;

    // ThreadPool for processing jobs (not loading)
    ThreadPool threadPool { 10 };
    ChangeBroadcaster& processBroadcaster;
    WaitableEvent signalEvent;
};
*/
