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
