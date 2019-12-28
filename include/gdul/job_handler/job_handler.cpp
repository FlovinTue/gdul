#include <gdul/job_handler/job_handler.h>
#include <gdul/job_handler/job_handler_impl.h>
#include <cassert>

namespace gdul
{
thread_local job job_handler::this_job(shared_ptr<jh_detail::job_impl>(nullptr));
thread_local worker job_handler::this_worker(&jh_detail::job_handler_impl::t_items.m_implicitWorker);

job_handler::job_handler()
	: job_handler(jh_detail::allocator_type())
{
}
job_handler::job_handler(jh_detail::allocator_type allocator)
	: m_allocator(allocator)
{

}
void job_handler::init() {
	jh_detail::allocator_type alloc(m_allocator);
	m_impl = gdul::make_shared<jh_detail::job_handler_impl, decltype(alloc)>(alloc, alloc);
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
worker job_handler::make_worker(const worker_info & info)
{
	return m_impl->make_worker(info);
}
std::size_t job_handler::num_workers() const
{
	return m_impl->num_workers();
}
std::size_t job_handler::num_enqueued() const
{
	return m_impl->num_enqueued();
}
concurrent_object_pool<jh_detail::batch_job_chunk_rep, jh_detail::allocator_type>* job_handler::get_batch_job_chunk_pool()
{
	return m_impl->get_batch_job_chunk_pool();
}
job job_handler::make_job(gdul::delegate<void()>&& workUnit)
{
	return m_impl->make_job(std::forward<delegate<void()>>(workUnit));
}
}
