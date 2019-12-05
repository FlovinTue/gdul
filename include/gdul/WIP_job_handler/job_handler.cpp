#include <gdul/WIP_job_handler/job_handler.h>
#include <gdul/WIP_job_handler/job_handler_impl.h>
#include <cassert>

namespace gdul
{
job_handler::job_handler()
	: job_handler(jh_detail::allocator_type())
{
}
job_handler::job_handler(const jh_detail::allocator_type & allocator)
	: m_allocator(allocator)
{
	m_impl = gdul::make_shared<jh_detail::job_handler_impl, jh_detail::allocator_type>(m_allocator);
}
job_handler::~job_handler()
{
}
void job_handler::retire_workers()
{
	m_impl->retire_workers();
}
worker job_handler::make_worker()
{
	return m_impl->make_worker();
}
std::size_t job_handler::num_workers() const
{
	return m_impl->num_workers();
}
std::size_t job_handler::num_enqueued() const
{
	return m_impl->num_enqueued();
}
job job_handler::make_job_internal(job_delegate<void, void>&& del)
{
	return m_impl->make_job(std::move(del));
}
}
