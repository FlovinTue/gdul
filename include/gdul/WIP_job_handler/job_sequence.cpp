#include <gdul\WIP_job_handler\job_handler.h>
#include <gdul\WIP_job_handler\job_sequence.h>
#include <gdul\WIP_job_handler\job_sequence_impl.h>

namespace gdul
{
job_sequence::job_sequence(job_handler* jobHandler)
	: myImpl(jobHandler->create_job_sequence())
{
}
job_sequence::job_sequence()
	: myImpl(nullptr)
{
}
job_sequence::~job_sequence()
{
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
void job_sequence::push(const std::function<void()>& inJob, Job_layer jobLayer)
{
	myImpl->push(inJob, jobLayer);
}
void job_sequence::push(std::function<void()>&& inJob, Job_layer jobLayer)
{
	myImpl->push(std::move(inJob), jobLayer);
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



