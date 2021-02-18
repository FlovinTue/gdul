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

#pragma once

#pragma warning(push)
#pragma warning(disable : 4324)

#include <chrono>
#include <atomic>
#include <thread>

#include <gdul/execution/job_handler/job_handler_utility.h>
#include <gdul/utility/delegate.h>
#include <gdul/execution/job_handler/worker/worker.h>
#include <gdul/execution/job_handler/job/job.h>
#include <gdul/execution/thread/thread.h>

namespace gdul {
namespace jh_detail
{
class job_handler_impl;

class alignas(64) worker_impl
{
public:
	using job_impl_shared_ptr = shared_ptr<job_impl>;

	worker_impl();
	worker_impl(thread&& thrd);
	~worker_impl();

	worker_impl& operator=(worker_impl&& other) noexcept;

	void set_sleep_threshhold(std::uint16_t ms);

	void enable();

	bool disable();

	void refresh_sleep_timer();

	bool is_sleepy() const;
	bool is_active() const;
	bool is_enabled() const;

	void set_run_on_enable(delegate<void()> && toCall);
	void set_run_on_disable(delegate<void()> && toCall);

	void add_assignment(job_queue* queue);

	void on_enable();
	void on_disable();

	void work();
	void idle();

	bool try_consume_from_once(job_queue* consumeFrom);

	const thread& get_thread() const;
	thread& get_thread();

private:
	void consume_job(job_impl_shared_ptr&& jb);
	typename job_impl_shared_ptr fetch_job();

	thread m_thread;

	gdul::delegate<void()> m_onEnable;
	gdul::delegate<void()> m_onDisable;

	std::chrono::high_resolution_clock::time_point m_lastJobTimepoint;
	std::chrono::high_resolution_clock m_sleepTimer;

	job_queue* m_targets[MaxWorkerTargets];

	std::uint16_t m_sleepThreshhold;

	std::atomic_bool m_isEnabled;
	std::atomic_bool m_isActive;
	std::atomic_uint8_t m_queuePushSync;
	std::atomic_uint8_t m_queueCount;

	std::uint8_t m_queueIndex;
};
}
}
#pragma warning(pop)