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

#pragma once

#pragma warning(push)
#pragma warning(disable : 4324)

#include <chrono>
#include <atomic>
#include <thread>

#include <gdul\WIP_job_handler\job_handler_commons.h>

namespace gdul {
namespace jh_detail
{

class job_handler_impl;

class alignas(64) worker_impl
{
public:
	worker_impl();
	worker_impl(std::thread&& thread, std::uint8_t coreAffinity);
	~worker_impl();

	worker_impl& operator=(worker_impl&& other) noexcept;

	void set_core_affinity(std::uint8_t core);
	void set_queue_affinity(std::uint8_t queue);
	void set_execution_priority(std::uint32_t priority);
	void set_sleep_threshhold(std::uint16_t ms);
	void set_name(const char* name);

	void activate();

	bool deactivate();

	void refresh_sleep_timer();

	bool is_sleepy() const;
	bool is_active() const;
	bool is_enabled() const;

	void idle();

	std::uint8_t get_queue_target();
	std::uint8_t get_fetch_retries() const;

private:
	std::thread m_thread;

	thread_handle m_threadHandle;
	
	std::size_t m_priorityDistributionIteration;

	std::chrono::high_resolution_clock::time_point m_lastJobTimepoint;

	std::uint16_t m_sleepThreshhold;

	std::chrono::high_resolution_clock m_sleepTimer;

	std::uint8_t m_autoCoreAffinity;
	std::uint8_t m_coreAffinity;
	std::uint8_t m_queueAffinity;

	std::atomic_bool m_isEnabled;
	std::atomic_bool m_isActive;
};
}
}
#pragma warning(pop)