#include "job_queue.h"
#include <gdul/job_handler/job_impl.h>

namespace gdul {

inline void job_async_queue::submit_job(jh_detail::job_impl_shared_ptr jb)
{
	m_queue.push(std::move(jb));
}
inline job_async_queue::job_async_queue(jh_detail::allocator_type alloc)
	: m_queue(alloc)
{
}
inline jh_detail::job_impl_shared_ptr job_async_queue::fetch_job()
{
	jh_detail::job_impl_shared_ptr out;
	m_queue.try_pop(out);
	return out;
}
inline job_sync_queue::job_sync_queue(jh_detail::allocator_type alloc)
	: m_queue(alloc)
{
}
inline void job_sync_queue::submit_job(jh_detail::job_impl_shared_ptr jb)
{
	m_queue.push(std::make_pair(jb->get_priority(), std::move(jb)));
}
inline jh_detail::job_impl_shared_ptr job_sync_queue::fetch_job()
{
	std::pair<float, jh_detail::job_impl_shared_ptr> out;
	m_queue.try_pop(out);
	return out.second;
}
}