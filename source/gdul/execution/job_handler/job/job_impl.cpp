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

#include <gdul/execution/job_handler/job/job_impl.h>
#include <gdul/execution/job_handler/job_handler_impl.h>
#include <gdul/execution/job_handler/job_handler.h>
#include <gdul/execution/job_handler/worker/worker.h>
#include <gdul/execution/job_handler/job_queue.h>
#include "job_impl.h"

namespace gdul {
namespace jh_detail {

job_impl::job_impl()
	: job_impl(delegate<void()>([]() {}), nullptr, nullptr, 0)
{
}
job_impl::job_impl(delegate<void()>&& workUnit, job_handler_impl* handler, job_queue* target, job_info* info)
	: m_workUnit(std::forward<delegate<void()>>(workUnit))
	, m_info(info)
	, m_completionTimer()
#if defined (GDUL_JOB_DEBUG)
	, m_enqueueTimer()
#endif
	, m_finished(false)
	, m_headDependee(nullptr)
	, m_handler(handler)
	, m_target(target)
	, m_dependencies(Job_Enable_Dependencies)
{
}
job_impl::~job_impl()
{
	assert(is_enabled() && "Job destructor ran before enable was called");
}

void job_impl::operator()()
{
	assert(!m_finished);

#if defined (GDUL_JOB_DEBUG)
	if (m_info) {
		m_info->m_enqueueTimeSet.log_time(m_enqueueTimer.elapsed());
	}
#endif

	m_completionTimer.start();

	m_workUnit();

	m_info->store_runtime(m_completionTimer.elapsed());

#if defined(GDUL_JOB_DEBUG)
	if (m_info)
		m_info->m_completionTimeSet.log_time(m_completionTimer.elapsed());
#endif

	m_finished.store(true, std::memory_order_seq_cst);

	detach_children();
}
bool job_impl::try_attach_child(job_impl_shared_ptr child)
{
	m_info->accumulate_dependant_time(child->get_remaining_propagation_time());

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
	if (job::this_job) {
		m_info->accumulate_dependant_time(job::this_job.m_impl->get_remaining_dependant_time());
	}

	GDUL_JOB_DEBUG_CONDTIONAL(timer waitTimer)
		while (!is_finished()) {
			if (!worker::this_worker.m_impl->try_consume_from_once(consumeFrom)) {
				jh_detail::job_handler_impl::t_items.this_worker_impl->refresh_sleep_timer();
				jh_detail::job_handler_impl::t_items.this_worker_impl->idle();
			}
		}
	GDUL_JOB_DEBUG_CONDTIONAL(if (m_info) m_info->m_waitTimeSet.log_time(waitTimer.elapsed()))
}
void job_impl::work_until_ready(job_queue* consumeFrom)
{
	if (job::this_job) {
		m_info->accumulate_propagation_time(job::this_job.m_impl->get_remaining_dependant_time());
	}
	GDUL_JOB_DEBUG_CONDTIONAL(timer waitTimer)
		while (!is_ready() && !is_enabled()) {
			if (!worker::this_worker.m_impl->try_consume_from_once(consumeFrom)) {
				jh_detail::job_handler_impl::t_items.this_worker_impl->refresh_sleep_timer();
				jh_detail::job_handler_impl::t_items.this_worker_impl->idle();
			}
		}
	GDUL_JOB_DEBUG_CONDTIONAL(if (m_info) m_info->m_waitTimeSet.log_time(waitTimer.elapsed()))
}
void job_impl::wait_until_finished() noexcept
{
	if (job::this_job) {
		m_info->accumulate_dependant_time(job::this_job.m_impl->get_remaining_dependant_time());
	}

	GDUL_JOB_DEBUG_CONDTIONAL(timer waitTimer)
		while (!is_finished()) {
			jh_detail::job_handler_impl::t_items.this_worker_impl->refresh_sleep_timer();
			jh_detail::job_handler_impl::t_items.this_worker_impl->idle();
		}
	GDUL_JOB_DEBUG_CONDTIONAL(if (m_info) m_info->m_waitTimeSet.log_time(waitTimer.elapsed()))
}
void job_impl::wait_until_ready() noexcept
{
	if (job::this_job) {
		m_info->accumulate_propagation_time(job::this_job.m_impl->get_remaining_dependant_time());
	}

	GDUL_JOB_DEBUG_CONDTIONAL(timer waitTimer)
		while (!is_ready() && !is_enabled()) {
			jh_detail::job_handler_impl::t_items.this_worker_impl->refresh_sleep_timer();
			jh_detail::job_handler_impl::t_items.this_worker_impl->idle();
		}
	GDUL_JOB_DEBUG_CONDTIONAL(if (m_info) m_info->m_waitTimeSet.log_time(waitTimer.elapsed()))
}
float job_impl::get_remaining_dependant_time() const noexcept
{
	return m_info->get_dependant_runtime() + m_info->get_runtime() - m_completionTimer.elapsed();
}
float job_impl::get_remaining_propagation_time() const noexcept
{
	return m_info->get_propagation_runtime() + m_info->get_runtime() - m_completionTimer.elapsed();
}
std::size_t job_impl::get_id() const noexcept
{
	return m_info->id();
}
void job_impl::set_info(job_info* info)
{
	m_info = info;
}
void job_impl::detach_children()
{
	detach_next(m_headDependee.exchange(job_node_shared_ptr(nullptr), std::memory_order_relaxed));
}
void job_impl::detach_next(job_node_shared_ptr from)
{
	if (!from) {
		return;
	}

	job_impl_shared_ptr dependant(std::move(from->m_job));

	detach_next(std::move(from->m_next));

	if (!dependant->remove_dependencies(1)) {
		job_queue* const target(dependant->get_target());

		target->submit_job(std::move(dependant));
	}
}
#if defined (GDUL_JOB_DEBUG)
void job_impl::on_enqueue() noexcept
{
	m_enqueueTimer.reset();
}
#endif
}
}
