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

#include <gdul/execution/job_handler/worker/worker_impl.h>
#include <gdul/execution/job_handler/job_handler_impl.h>
#include <gdul/execution/job_handler/job_queue.h>
#include <gdul/execution/job_handler/job/job.h>
#include <gdul/execution/job_handler/job/job_impl.h>

#include <cassert>
#include <algorithm>
#include <cmath>

#if defined(_WIN64) | defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace gdul
{
namespace jh_detail
{

worker_impl::worker_impl()
	: m_thread()
	, m_onEnable([](){})
	, m_onDisable([](){})
	, m_isEnabled(false)
	, m_targets{}
	, m_sleepThreshhold(std::numeric_limits<std::uint16_t>::max())
	, m_isActive(false)
	, m_queuePushSync(0)
	, m_queueCount(0)
	, m_queueIndex(0)
{
}
worker_impl::worker_impl(thread&& thrd)
	: m_thread()
	, m_onEnable([]() {})
	, m_onDisable([]() {})
	, m_isEnabled(false)
	, m_targets{}
	, m_sleepThreshhold(80)
	, m_isActive(false)
	, m_queuePushSync(0)
	, m_queueCount(0)
	, m_queueIndex(0)
{
	m_thread.swap(thrd);

	m_isActive.store(true, std::memory_order_release);
}

worker_impl::~worker_impl()
{
	disable();

	if (m_thread.joinable()) {
		m_thread.join();
	}
}
worker_impl & worker_impl::operator=(worker_impl && other) noexcept
{
	m_thread.swap(other.m_thread);
	m_onEnable = std::move(other.m_onEnable);
	m_onDisable = std::move(other.m_onDisable);
	m_queueCount = other.m_queueCount.load(std::memory_order_relaxed);
	m_queuePushSync = other.m_queuePushSync.load(std::memory_order_relaxed);
	m_isEnabled.store(other.m_isEnabled.load(std::memory_order_relaxed), std::memory_order_relaxed);
	m_sleepThreshhold = other.m_sleepThreshhold;
	m_sleepTimer = other.m_sleepTimer;
	m_lastJobTimepoint = other.m_lastJobTimepoint;
	m_isActive.store(other.m_isActive.load(std::memory_order_relaxed), std::memory_order_release);
	m_queueIndex = other.m_queueIndex;
	std::copy(std::begin(other.m_targets), std::end(other.m_targets), m_targets);

	return *this;
}

void worker_impl::set_sleep_threshhold(std::uint16_t ms)
{
	m_sleepThreshhold = ms;
}
void worker_impl::enable()
{
	m_isEnabled.store(true, std::memory_order_release);
}
bool worker_impl::disable()
{
	return m_isActive.exchange(false, std::memory_order_release);
}
void worker_impl::refresh_sleep_timer()
{
	m_lastJobTimepoint = m_sleepTimer.now();
}
bool worker_impl::is_sleepy() const
{
	const std::chrono::high_resolution_clock::time_point current(m_sleepTimer.now());

	return !(std::chrono::duration_cast<std::chrono::milliseconds>(current - m_lastJobTimepoint).count() < m_sleepThreshhold);
}
bool worker_impl::is_active() const
{
	return m_isActive.load(std::memory_order_relaxed);
}
bool worker_impl::is_enabled() const
{
	return m_isEnabled.load(std::memory_order_relaxed);
}
void worker_impl::set_run_on_enable(delegate<void()>&& toCall)
{
	assert(!is_enabled() && "cannot set_run_on_enable after worker has been enabled");
	m_onEnable = std::move(toCall);
}
void worker_impl::set_run_on_disable(delegate<void()>&& toCall)
{
	assert(is_active() && "cannot set_run_on_disable after worker has been retired");
	m_onDisable = std::move(toCall);
}
void worker_impl::add_assignment(job_queue* queue)
{
	const std::uint8_t ix(m_queuePushSync++);
	assert(ix < Max_Worker_Targets && "Max worker targets exceeded");

	m_targets[ix] = queue;

	m_queueCount++;

	queue->m_assignees.fetch_add(1, std::memory_order_relaxed);
}
void worker_impl::on_enable()
{
	m_onEnable();
}
void worker_impl::on_disable()
{
	m_onDisable();
}
void worker_impl::idle()
{
	if (is_sleepy()) {
		std::this_thread::sleep_for(std::chrono::microseconds(10));
	}
	else {
		std::this_thread::yield();
	}
}
void worker_impl::work()
{
	while (is_active()) {

		if (job_impl_shared_ptr jb = fetch_job()) {
			consume_job(std::move(jb));
		}
		else {
			idle();
		}
	}
}
bool worker_impl::try_consume_from_once(job_queue* consumeFrom)
{
	if (job_impl_shared_ptr jb = consumeFrom->fetch_job()) {

		consume_job(std::move(jb));

		return true;
	}

	return false;
}
const thread& worker_impl::get_thread() const
{
	return m_thread;
}
thread& worker_impl::get_thread()
{
	return m_thread;
}
void worker_impl::consume_job(job_impl_shared_ptr&& jb)
{
	job swap(std::move(job::this_job));

	job::this_job = job(std::move(jb));
	job::this_job.m_impl->operator()();
	job::this_job = std::move(swap);

	job_handler_impl::t_items.this_worker_impl->refresh_sleep_timer();
}
typename worker_impl::job_impl_shared_ptr worker_impl::fetch_job()
{
	const std::uint8_t queueCount(m_queueCount.load(std::memory_order_relaxed));

	for (std::uint8_t i = 0; i < queueCount; ++i) {
		const std::uint8_t ix(m_queueIndex++ % queueCount);
		if (job_impl_shared_ptr out = m_targets[ix]->fetch_job()) {
			return out;
		}
	}

	return job_impl_shared_ptr(nullptr);
}
}
}