// Copyright(c) 2019 Flovin Michaelsen
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

#include <gdul/job_handler/worker_impl.h>
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
	: m_threadHandle(jh_detail::create_thread_handle())
	, m_queueDistributionIteration(0)
	, m_isEnabled(false)
	, m_allocator(allocator_type())
	, m_sleepThreshhold(std::numeric_limits<std::uint16_t>::max())
	, m_isActive(false)
{
	m_distributionChunks = jh_detail::pow2summation(m_info.m_queueBegin + 1, m_info.m_queueEnd);
}
worker_impl::worker_impl(worker_info&& info, std::thread&& thrd, allocator_type allocator)
	: m_thread(std::move(thrd))
	, m_info(std::move(info))
	, m_threadHandle(m_thread.native_handle())
	, m_queueDistributionIteration(0)
	, m_isEnabled(false)
	, m_allocator(allocator)
	, m_sleepThreshhold(250)
	, m_isActive(false)
{
	m_info.m_queueEnd = std::min<std::uint8_t>(m_info.m_queueEnd, Num_Job_Queues);
	m_info.m_queueBegin = std::min<std::uint8_t>(m_info.m_queueBegin, m_info.m_queueEnd - 1);

	m_distributionChunks = jh_detail::pow2summation(m_info.m_queueBegin + 1, m_info.m_queueEnd);

	jh_detail::set_thread_core_affinity(m_info.m_coreAffinity, m_threadHandle);

	m_isActive.store(true, std::memory_order_release);
}

worker_impl::~worker_impl()
{
	if (m_thread.get_id() == std::thread().get_id()) {
		CloseHandle(m_threadHandle);
		m_threadHandle = nullptr;
	}

	disable();

	if (m_thread.joinable()) {
		m_thread.join();
	}
}
worker_impl & worker_impl::operator=(worker_impl && other) noexcept
{
	m_thread.swap(other.m_thread);
	m_info = other.m_info;
	m_isEnabled.store(other.m_isEnabled.load(std::memory_order_relaxed), std::memory_order_relaxed);
	std::swap(m_threadHandle, other.m_threadHandle);
	m_sleepThreshhold = other.m_sleepThreshhold;
	m_sleepTimer = other.m_sleepTimer;
	m_queueDistributionIteration = other.m_queueDistributionIteration;
	m_lastJobTimepoint = other.m_lastJobTimepoint;
	m_isActive.store(other.m_isActive.load(std::memory_order_relaxed), std::memory_order_release);

	return *this;
}

void worker_impl::set_core_affinity(std::uint8_t core)
{
	assert(is_active() && "Cannot set affinity to inactive worker");

	m_info.m_coreAffinity = core;

	const uint8_t core_(m_info.m_coreAffinity % std::thread::hardware_concurrency());

	jh_detail::set_thread_core_affinity(core_, m_threadHandle);
}
void worker_impl::set_execution_priority(std::uint32_t priority)
{
	assert(is_active() && "Cannot set execution priority to inactive worker");

	jh_detail::set_thread_priority(priority, m_threadHandle);
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
	const std::chrono::high_resolution_clock::time_point delta(current - m_lastJobTimepoint);

	return !(std::chrono::duration_cast<std::chrono::milliseconds>(current - delta).count() < m_sleepThreshhold);
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
	m_info.m_onEnable = std::forward<delegate<void()>>(toCall);
}
void worker_impl::set_run_on_disable(delegate<void()>&& toCall)
{
	m_info.m_onDisable = std::forward<delegate<void()>>(toCall);
}
void worker_impl::on_enable()
{
	m_info.m_onEnable();
}
void worker_impl::on_disable()
{
	m_info.m_onDisable();
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
// Maybe find some way to stick around at an index for a while? Better for cache...
// Also, maybe avoid retrying at a failed index twice in a row.
std::uint8_t worker_impl::get_queue_target()
{
	const std::size_t iteration(++m_queueDistributionIteration);

	uint8_t index(0);

	const std::uint8_t range(m_info.m_queueEnd - m_info.m_queueBegin);

	// Find way to remove loop.
	// Maybe find the highest mod in one check somehow?
	for (uint8_t i = 1; i < range; ++i) {
		const std::uint8_t power(((range) - (i + 1)));
		const float fdesiredSlice(std::powf((float)2, (float)power));
		const std::size_t desiredSlice((std::size_t)fdesiredSlice);
		const std::size_t awardedSlice((m_distributionChunks) / desiredSlice);
		const uint8_t prev(index);
		const uint8_t eval(iteration % awardedSlice == 0);
		index += eval * i;
		index -= prev * eval;
	}

	return m_info.m_queueBegin + index;
}
std::uint8_t worker_impl::get_fetch_retries() const
{
	return m_info.m_queueEnd - m_info.m_queueBegin;
}
allocator_type worker_impl::get_allocator() const
{
	return m_allocator;
}
void worker_impl::set_name(const std::string& name)
{
#if defined(GDUL_DEBUG)
	assert(is_active() && "Cannot set name to inactive worker");

	jh_detail::set_thread_name(name.c_str(), m_threadHandle);
	
	m_name = name;
#else
	(void)name;
#endif
}
}
}