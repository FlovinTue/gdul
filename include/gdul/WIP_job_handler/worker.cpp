#include <gdul\WIP_job_handler\worker.h>
#include <gdul\WIP_job_handler\worker_impl.h>

namespace gdul
{
worker::worker(worker_impl * impl)
	: myImpl(impl)
{
}

worker::~worker()
{
}

void worker::set_core_affinity(std::uint8_t core)
{
	myImpl->set_core_affinity(core);
}

void worker::set_queue_affinity(std::uint8_t queue)
{
	myImpl->set_queue_affinity(queue);
}

void worker::set_execution_priority(uint32_t priority)
{
	myImpl->set_execution_priority(priority);
}

void worker::set_name(const char * name)
{
	myImpl->set_name(name);
}
bool worker::retire()
{
	return myImpl->retire();
}
bool worker::is_retired() const
{
	return myImpl->is_retired();
}
}
