#include <gdul\WIP_job_handler\Job.h>
#include <gdul\WIP_job_handler\job_sequence_impl.h>
#include <gdul\WIP_job_handler\job_handler.h>

namespace gdul
{
job::job()
	: myWorkItem([]() {})
{
}

job::job(std::function<void()>&& callable)
	: myWorkItem(std::move(callable))
{
}


job::~job()
{
}
void job::operator()() const
{
	myWorkItem();

	if (job_handler::this_JobSequence) {
		job_handler::this_JobSequence->advance();
	}
}
}
