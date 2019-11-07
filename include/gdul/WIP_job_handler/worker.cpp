#include <gdul\WIP_job_handler\worker.h>
#include <gdul\WIP_job_handler\worker_impl.h>

namespace gdul
{
worker::worker(job_handler_detail::worker_impl * impl)
	: m_impl(impl)
{
}

worker::~worker()
{
}

void worker::set_core_affinity(std::uint8_t core)
{
	m_impl->set_core_affinity(core);
}

void worker::set_queue_affinity(std::uint8_t queue)
{
	m_impl->set_queue_affinity(queue);
}

void worker::set_execution_priority(std::int32_t priority)
{
	m_impl->set_execution_priority(priority);
}

void worker::set_name(const char * name)
{
	m_impl->set_name(name);
}
void worker::activate()
{
	m_impl->activate();
}
bool worker::deactivate()
{
	return m_impl->deactivate();
}
bool worker::is_active() const
{
	return m_impl->is_active();
}
}
