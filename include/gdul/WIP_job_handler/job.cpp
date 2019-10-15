#include <gdul\WIP_job_handler\Job.h>
#include <gdul\WIP_job_handler\job_handler.h>

namespace gdul
{
job::job()
{
}

job::job(std::function<void()>&& callable)
	: myCallable(std::move(callable))
	, mySequenceKey(0)
{
}

job::job(std::function<void()>&& callable, const job & dependency)
	: myCallable(std::move(callable))
	, mySequenceKey(dependency.mySequenceKey - 1)
{

}


job::~job()
{
}
void job::operator()() const
{
	
}
}
