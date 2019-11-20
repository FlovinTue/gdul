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

#include <gdul\WIP_job_handler\worker_impl.h>
#include <cassert>
#include <cmath>

#if defined(_WIN64) | defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace gdul
{
namespace job_handler_detail
{

worker_impl::worker_impl()
	: m_autoCoreAffinity(Worker_Auto_Affinity)
	, m_coreAffinity(Worker_Auto_Affinity)
	, m_isEnabled(false)
	, m_priorityDistributionIteration(0)
	, m_queueAffinity(Worker_Auto_Affinity)
	, m_sleepThreshhold(std::numeric_limits<std::uint16_t>::max())
	, m_threadHandle(job_handler_detail::create_thread_handle())
	, m_isActive(false)
{
}
worker_impl::worker_impl(std::thread&& thread, std::uint8_t coreAffinity)
	: m_autoCoreAffinity(coreAffinity)
	, m_thread(std::move(thread))
	, m_coreAffinity(Worker_Auto_Affinity)
	, m_isEnabled(false)
	, m_priorityDistributionIteration(0)
	, m_queueAffinity(Worker_Auto_Affinity)
	, m_sleepThreshhold(250)
	, m_threadHandle(m_thread.native_handle())
	, m_isActive(false)
{
	job_handler_detail::set_thread_core_affinity(coreAffinity, m_threadHandle);

	m_isActive.store(true, std::memory_order_release);
}

worker_impl::~worker_impl()
{
	if (m_thread.get_id() == std::thread().get_id()) {
		CloseHandle(m_threadHandle);
		m_threadHandle = nullptr;
	}

	deactivate();

	if (m_thread.joinable()) {
		m_thread.join();
	}
}
worker_impl & worker_impl::operator=(worker_impl && other) noexcept
{
	m_autoCoreAffinity = other.m_autoCoreAffinity;
	m_thread.swap(other.m_thread);
	m_coreAffinity = other.m_coreAffinity;
	m_isEnabled.store(other.m_isEnabled.load(std::memory_order_relaxed), std::memory_order_relaxed);
	std::swap(m_threadHandle, other.m_threadHandle);
	m_sleepThreshhold = other.m_sleepThreshhold;
	m_sleepTimer = other.m_sleepTimer;
	m_priorityDistributionIteration = other.m_priorityDistributionIteration;
	m_queueAffinity = other.m_queueAffinity;
	m_lastJobTimepoint = other.m_lastJobTimepoint;
	m_isActive.store(other.m_isActive.load(std::memory_order_relaxed), std::memory_order_release);

	return *this;
}

void worker_impl::set_core_affinity(std::uint8_t core)
{
	assert(is_active() && "Cannot set affinity to inactive worker");

	m_coreAffinity = core;

	if (m_coreAffinity == job_handler_detail::Worker_Auto_Affinity) {
		m_coreAffinity = m_autoCoreAffinity;
	}

	const uint8_t core_(m_coreAffinity % std::thread::hardware_concurrency());

	job_handler_detail::set_thread_core_affinity(core_, m_threadHandle);
}
void worker_impl::set_queue_affinity(std::uint8_t queue)
{
	m_queueAffinity = queue;
}
void worker_impl::set_execution_priority(std::uint32_t priority)
{
	assert(is_active() && "Cannot set execution priority to inactive worker");

	job_handler_detail::set_thread_priority(priority, m_threadHandle);
}
void worker_impl::set_sleep_threshhold(std::uint16_t ms)
{
	m_sleepThreshhold = ms;
}
void worker_impl::activate()
{
	m_isEnabled.store(true, std::memory_order_release);
}
bool worker_impl::deactivate()
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
	if (m_queueAffinity != job_handler_detail::Worker_Auto_Affinity) {
		return m_queueAffinity;
	}

	constexpr std::size_t totalDistributionChunks(job_handler_detail::pow2summation(1, job_handler_detail::Priority_Granularity));

	const std::size_t iteration(++m_priorityDistributionIteration);

	uint8_t index(0);


	// Find way to remove loop.
	// Maybe find the highest mod in one check somehow?
	for (uint8_t i = 1; i < job_handler_detail::Priority_Granularity; ++i) {
		const std::uint8_t power(((job_handler_detail::Priority_Granularity) - (i + 1)));
		const float fdesiredSlice(std::powf((float)2, (float)power));
		const std::size_t desiredSlice((std::size_t)fdesiredSlice);
		const std::size_t awardedSlice((totalDistributionChunks) / desiredSlice);
		const uint8_t prev(index);
		const uint8_t eval(iteration % awardedSlice == 0);
		index += eval * i;
		index -= prev * eval;
	}

	return index;
}
std::uint8_t worker_impl::get_fetch_retries() const
{
	return m_queueAffinity == job_handler_detail::Worker_Auto_Affinity ? job_handler_detail::Priority_Granularity : 1;
}
void worker_impl::set_name(const char * name)
{
	assert(is_active() && "Cannot set name to inactive worker");

	job_handler_detail::set_thread_name(name, m_threadHandle);
}
}
}