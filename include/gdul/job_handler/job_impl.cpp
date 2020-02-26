// Copyright(c) 2020 Flovin Michaelsen
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <gdul/job_handler/job_impl.h>
#include <gdul/job_handler/job_handler_impl.h>
#include <gdul/concurrent_object_pool/concurrent_object_pool.h>
#include <gdul/job_handler/job_handler.h>

namespace gdul
{
namespace jh_detail
{
job_impl::job_impl()
	: job_impl(delegate<void()>([]() {}), nullptr)
{
}
job_impl::job_impl(delegate<void()>&& workUnit, job_handler_impl* handler)
	: m_workUnit(std::forward<delegate<void()>>(workUnit))
	, m_finished(false)
	, m_enabled(false)
	, m_firstDependee(nullptr)
	, m_targetQueue(Default_Job_Queue)
	, m_handler(handler)
	, m_dependencies(Job_Max_Dependencies)
#if defined (GDUL_JOB_DEBUG)
	, m_trackingNode(nullptr)
#endif
{
}
job_impl::~job_impl()
{
}

void job_impl::operator()()
{
	assert(!m_finished);

#if defined (GDUL_JOB_DEBUG)
	if (m_trackingNode)
		job_handler::this_job.m_debugId = m_trackingNode->m_id;

	timer time;
#endif
	m_workUnit();
#if defined(GDUL_JOB_DEBUG)
	//if (m_trackingNode)
	//	m_trackingNode->log_time(m_timer.get());
#endif

	m_finished.store(true, std::memory_order_seq_cst);

	detach_children();
}
bool job_impl::try_attach_child(job_impl_shared_ptr child)
{
	typename job_handler_impl::job_node_allocator jobDependeeAlloc(m_handler->get_job_node_chunk_pool());

	job_node_shared_ptr dependee(gdul::allocate_shared<job_node>(jobDependeeAlloc));
	dependee->m_job = std::move(child);

	job_node_shared_ptr firstDependee(nullptr);
	job_node_raw_ptr rawRep(nullptr);
	do {
		firstDependee = m_firstDependee.load(std::memory_order_relaxed);
		rawRep = firstDependee;

		dependee->m_next = std::move(firstDependee);

		if (m_finished.load(std::memory_order_seq_cst)) {
			return false;
		}

	} while (!m_firstDependee.compare_exchange_strong(rawRep, std::move(dependee), std::memory_order_relaxed, std::memory_order_relaxed));

	return true;
}
job_queue job_impl::get_target_queue() const noexcept
{
	return m_targetQueue;
}
void job_impl::set_target_queue(job_queue target) noexcept
{
	m_targetQueue = target;
}
bool job_impl::try_add_dependencies(std::uint32_t n)
{
	std::uint32_t exp(m_dependencies.load(std::memory_order_relaxed));
	while (exp != 0 && !m_dependencies.compare_exchange_weak(exp, exp + n, std::memory_order_relaxed));

	return exp != 0;
}
std::uint32_t job_impl::remove_dependencies(std::uint32_t n)
{
	std::uint32_t result(m_dependencies.fetch_sub(n, std::memory_order_acq_rel));
	return result - n;
}
bool job_impl::enable()
{
	if (!m_enabled.exchange(true, std::memory_order_relaxed)){
		return !remove_dependencies(Job_Max_Dependencies);
	}
	return false;
}
job_handler_impl * job_impl::get_handler() const
{
	return m_handler;
}
bool job_impl::is_finished() const
{
	return m_finished.load(std::memory_order_relaxed);
}
bool job_impl::is_enabled() const
{
	return m_enabled.load(std::memory_order_relaxed);
}
void job_impl::work_until_finished(job_queue consumeFrom)
{
	while (!is_finished()) {
		if (!m_handler->try_consume_from_once(consumeFrom)) {
			jh_detail::job_handler_impl::t_items.this_worker_impl->refresh_sleep_timer();
			jh_detail::job_handler_impl::t_items.this_worker_impl->idle();
		}
	}
}
void job_impl::wait_until_finished() noexcept
{
	while (!is_finished()) {
		jh_detail::job_handler_impl::t_items.this_worker_impl->refresh_sleep_timer();
		jh_detail::job_handler_impl::t_items.this_worker_impl->idle();
	}
}
void job_impl::detach_children()
{
	job_node_shared_ptr dependee(m_firstDependee.exchange(job_node_shared_ptr(nullptr), std::memory_order_relaxed));

	while (dependee) {
		job_node_shared_ptr next(std::move(dependee->m_next));

		if (!dependee->m_job->remove_dependencies(1)) {
			job_handler_impl* const nativeHandler(dependee->m_job->get_handler());
			nativeHandler->enqueue_job(std::move(dependee->m_job));
		}

		dependee = std::move(next);
	}
}
#if defined(GDUL_JOB_DEBUG)
void job_impl::register_tracking_node(constexpr_id id, const char* name) noexcept
{
	m_trackingNode = job_tracker::register_node(id, name);
}
#endif
}
}
