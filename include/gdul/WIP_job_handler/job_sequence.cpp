#include <gdul\WIP_job_handler\job_handler.h>
#include <gdul\WIP_job_handler\job_sequence.h>
#include <gdul\WIP_job_handler\job_sequence_impl.h>

namespace gdul
{
job_sequence::job_sequence(job_handler* jobHandler)
	: myImpl(jobHandler->create_job_sequence())
	, myHandler(jobHandler)
{
}
job_sequence::job_sequence()
	: myImpl(nullptr)
	, myHandler(nullptr)
{
}
job_sequence::~job_sequence()
{
	myImpl->return_to_pool();
}
job_sequence::job_sequence(job_sequence && other)
	: job_sequence()
{
	operator=(std::move(other));
}
job_sequence & job_sequence::operator=(job_sequence && other)
{
	std::swap(other.myImpl, myImpl);

	return *this;
}
void job_sequence::swap(job_sequence & other)
{
	std::swap(myImpl, other.myImpl);
}
void job_sequence::swap(job_sequence && other)
{
	swap(other);
}
void job_sequence::run_synchronous(job && job)
{
	myImpl->run_synchronous(std::move(job));
}
void job_sequence::run_synchronous(const job & job)
{
	myImpl->run_synchronous(job);
}
void job_sequence::run_asynchronous(job && job)
{
	
}
void job_sequence::run_asynchronous(const job & job)
{
}
void job_sequence::pause()
{
	myImpl->pause();
}
void job_sequence::resume()
{
	myImpl->resume();
}
}



