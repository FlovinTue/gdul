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

#include <gdul/job_handler/job/job_impl.h>
#include <gdul/job_handler/job_handler_impl.h>
#include <gdul/job_handler/job_handler.h>
#include <gdul/job_handler/worker/worker.h>
#include <gdul/job_handler/job_queue.h>
#include <gdul/job_handler/job/job_node.h>
#include "job_impl.h"

namespace gdul {
namespace jh_detail {
job_impl::job_impl()
	: job_impl(delegate<void()>([]() {}), nullptr, nullptr, 0)
{
}
#if !defined(GDUL_JOB_DEBUG)
job_impl::job_impl(delegate<void()>&& workUnit, job_handler_impl* handler, job_queue* target, job_info* info)
	: m_info(info)
	, m_workUnit(std::forward<delegate<void()>>(workUnit))
	, m_finished(false)
	, m_headDependee(nullptr)
	, m_handler(handler)
	, m_target(target)
	, m_dependencies(Job_Enable_Dependencies)
{
}
#else
job_impl::job_impl(delegate<void()>&& workUnit, job_handler_impl* handler, job_queue* target, job_info* info, const char* name, const char* file, std::size_t line)
	: m_info(info)
	, m_workUnit(std::forward<delegate<void()>>(workUnit))
	, m_finished(false)
	, m_headDependee(nullptr)
	, m_handler(handler)
	, m_target(target)
	, m_dependencies(Job_Enable_Dependencies)
{
}
#endif
job_impl::~job_impl()
{
	assert(is_enabled() && "Job destructor ran before enable was called");
}

void job_impl::operator()()
{
	assert(!m_finished);

#if defined (GDUL_JOB_DEBUG)
	const std::size_t swap(job::this_job.m_persistentId);
	if (m_info) {
		m_info->m_enqueueTimeSet.log_time(m_enqueueTimer.get());
		job::this_job.m_persistentId = m_persistentId;
	}
#endif

	timer completionTimer;

	m_workUnit();

	m_info->store_accumulated_priority(completionTimer.get());

#if defined(GDUL_JOB_DEBUG)
	if (m_info)
		m_info->m_completionTimeSet.log_time(completionTimer.get());

	job::this_job.m_persistentId = swap;
#endif

	m_finished.store(true, std::memory_order_seq_cst);

	detach_children();
}
bool job_impl::try_attach_child(job_impl_shared_ptr child)
{
	pool_allocator<job_node> alloc(m_handler->get_job_node_allocator());

	job_node_shared_ptr dependee(gdul::allocate_shared<job_node>(alloc));
	dependee->m_job = std::move(child);

	job_node_shared_ptr firstDependee(nullptr);
	job_node_raw_ptr rawRep(nullptr);
	do {
		firstDependee = m_headDependee.load(std::memory_order_relaxed);
		rawRep = firstDependee;

		dependee->m_next = std::move(firstDependee);

		if (m_finished.load(std::memory_order_seq_cst)) {
			return false;
		}

	} while (!m_headDependee.compare_exchange_strong(rawRep, std::move(dependee), std::memory_order_release, std::memory_order_relaxed));

	return true;
}
bool job_impl::try_add_dependencies(std::uint32_t n)
{
	std::uint32_t exp(m_dependencies.load(std::memory_order_relaxed));
	do {
		assert(!(std::numeric_limits<std::uint32_t>::max() < (std::size_t(exp) + std::size_t(n))) && "Exceeding job max dependencies");

		if ((std::numeric_limits<std::uint32_t>::max() < (std::size_t(exp) + std::size_t(n))))
			return false;

	} while (exp != 0 && !m_dependencies.compare_exchange_weak(exp, exp + n, std::memory_order_relaxed));

	return exp;
}
std::uint32_t job_impl::remove_dependencies(std::uint32_t n)
{
	std::uint32_t result(m_dependencies.fetch_sub(n, std::memory_order_acq_rel));
	return result - n;
}
enable_result job_impl::enable() noexcept
{
	std::uint32_t exp(m_dependencies.load(std::memory_order_relaxed));

	while (!(exp < Job_Enable_Dependencies)) {
		if (m_dependencies.compare_exchange_weak(exp, exp - Job_Enable_Dependencies, std::memory_order_relaxed)) {
			std::uint8_t result(enable_result_enabled);
			result |= enable_result_enqueue * !(exp - Job_Enable_Dependencies);
			return (enable_result)result;
		}
	}

	return enable_result_null;
}
bool job_impl::enable_if_ready() noexcept
{
	std::uint32_t exp(m_dependencies.load(std::memory_order_relaxed));

	if (Job_Enable_Dependencies == exp)
		return m_dependencies.compare_exchange_strong(exp, 0, std::memory_order_relaxed);

	return false;
}
job_queue* job_impl::get_target() const noexcept
{
	return m_target;
}
bool job_impl::is_finished() const noexcept
{
	return m_finished.load(std::memory_order_relaxed);
}
bool job_impl::is_enabled() const noexcept
{
	return m_dependencies.load(std::memory_order_relaxed) < Job_Enable_Dependencies;
}
bool job_impl::is_ready() const noexcept
{
	const std::uint32_t dependencies(m_dependencies.load(std::memory_order_relaxed));

	return dependencies == Job_Enable_Dependencies;
}
void job_impl::work_until_finished(job_queue* consumeFrom)
{
	GDUL_JOB_DEBUG_CONDTIONAL(timer waitTimer)
		while (!is_finished()) {
			if (!worker::this_worker.m_impl->try_consume_from_once(consumeFrom)) {
				jh_detail::job_handler_impl::t_items.this_worker_impl->refresh_sleep_timer();
				jh_detail::job_handler_impl::t_items.this_worker_impl->idle();
			}
		}
	GDUL_JOB_DEBUG_CONDTIONAL(if (m_info) m_info->m_waitTimeSet.log_time(waitTimer.get()))
}
void job_impl::work_until_ready(job_queue* consumeFrom)
{
	GDUL_JOB_DEBUG_CONDTIONAL(timer waitTimer)
		while (!is_ready() && !is_enabled()) {
			if (!worker::this_worker.m_impl->try_consume_from_once(consumeFrom)) {
				jh_detail::job_handler_impl::t_items.this_worker_impl->refresh_sleep_timer();
				jh_detail::job_handler_impl::t_items.this_worker_impl->idle();
			}
		}
	GDUL_JOB_DEBUG_CONDTIONAL(if (m_info) m_info->m_waitTimeSet.log_time(waitTimer.get()))
}
void job_impl::wait_until_finished() noexcept
{
	GDUL_JOB_DEBUG_CONDTIONAL(timer waitTimer)
		while (!is_finished()) {
			jh_detail::job_handler_impl::t_items.this_worker_impl->refresh_sleep_timer();
			jh_detail::job_handler_impl::t_items.this_worker_impl->idle();
		}
	GDUL_JOB_DEBUG_CONDTIONAL(if (m_info) m_info->m_waitTimeSet.log_time(waitTimer.get()))
}
void job_impl::wait_until_ready() noexcept
{
	GDUL_JOB_DEBUG_CONDTIONAL(timer waitTimer)
		while (!is_ready() && !is_enabled()) {
			jh_detail::job_handler_impl::t_items.this_worker_impl->refresh_sleep_timer();
			jh_detail::job_handler_impl::t_items.this_worker_impl->idle();
		}
	GDUL_JOB_DEBUG_CONDTIONAL(if (m_info) m_info->m_waitTimeSet.log_time(waitTimer.get()))
}
float job_impl::get_priority() const noexcept
{
	return 0.0f;
}
std::size_t job_impl::get_id() const noexcept
{
	return m_info->id();
}
void job_impl::detach_children()
{
	job_node_shared_ptr dependee(m_headDependee.exchange(job_node_shared_ptr(nullptr), std::memory_order_relaxed));

	while (dependee) {
		job_node_shared_ptr next(std::move(dependee->m_next));

		if (!dependee->m_job->remove_dependencies(1)) {
			job_queue* const target(dependee->m_job->get_target());

			target->submit_job(std::move(dependee->m_job));
		}

		dependee = std::move(next);
	}
}
#if defined(GDUL_JOB_DEBUG)
job_info* job_impl::get_job_info(std::size_t id, const char* name, const char* file, std::uint32_t line, bool batchSub)
{
	job_graph& graph(m_handler->get_job_graph());

	m_info = !batchSub ? graph.get_job_info(id, name, file, line) : graph.get_job_info_sub(id, name);
	return m_info->id();
}
void job_impl::on_enqueue() noexcept
{
	m_enqueueTimer.reset();
}
#endif
}
}
