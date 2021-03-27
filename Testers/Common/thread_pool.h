#pragma once

#include <functional>
#include <atomic>
#include <concurrent_queue.h>
#include <gdul/containers/concurrent_queue.h>
#include <vector>
#include <gdul/WIP/qsbr.h>
#include <gdul/execution/thread/thread.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#undef max

namespace gdul
{

class thread_pool
{
public:
	thread_pool(std::uint32_t coreMask = std::numeric_limits<uint32_t>::max());
	~thread_pool();

	void add_task(std::function<void()> task);
	void decommission();

	bool has_unfinished_tasks() const;

	std::size_t worker_count() const;
private:
	void idle(std::uint64_t affinityMask);

	uint32_t spin();
	void release();

	std::vector<gdul::thread> m_threads;

	concurrency::concurrent_queue<std::function<void()>> m_taskQueue;

	std::atomic<bool> m_inCommission;

	std::atomic<std::uint32_t> m_taskCounter;
};

thread_pool::thread_pool(std::uint32_t coreMask) :
	m_inCommission(true),
	m_taskCounter(0)
{
	for (std::uint32_t i = 0; i < 32 && i < std::thread::hardware_concurrency(); ++i) {
		const std::uint64_t mask(1ull << i);

		if (coreMask & mask) {
			m_threads.push_back(gdul::thread(&thread_pool::idle, this, mask));
		}
	}
}


thread_pool::~thread_pool()
{
	decommission();
}

void thread_pool::add_task(std::function<void()> task)
{
	++m_taskCounter;
	m_taskQueue.push(task);
}

void thread_pool::decommission()
{
	m_inCommission = false;

	for (size_t i = 0; i < m_threads.size(); ++i)
		m_threads[i].join();

	m_threads.clear();
}
std::size_t thread_pool::worker_count() const
{
	return m_threads.size();
}
bool thread_pool::has_unfinished_tasks() const
{
	return 0 < m_taskCounter.load(std::memory_order_acquire);
}
void thread_pool::idle(std::uint64_t affinityMask)
{
	//qsbr::register_thread();

	uint64_t result(0);

	do
	{
		result = SetThreadAffinityMask(GetCurrentThread(), affinityMask);
	} while (!result);


	while (m_inCommission.load(std::memory_order_relaxed) || (0 < m_taskCounter.load(std::memory_order_acquire)))
	{
		std::function<void()> task;
		if (m_taskQueue.try_pop(task))
		{
			task();
			--m_taskCounter;
		}
		else
		{
			std::this_thread::yield();
		}
	}

	//qsbr::unregister_thread();
}
}