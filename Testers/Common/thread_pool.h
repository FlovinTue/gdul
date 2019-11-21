#pragma once

#include <functional>
#include <thread>
#include <atomic>
#include <concurrent_queue.h>
#include <gdul/concurrent_queue/concurrent_queue.h>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace gdul
{

class thread_pool
{
public:
	thread_pool(std::uint32_t threads = std::thread::hardware_concurrency(), std::uint32_t affinityBegin = 0);
	~thread_pool();

	void add_task(std::function<void()> task);
	void decommission();

	bool has_unfinished_tasks() const;
private:
	void idle(std::uint64_t affinityMask);

	uint32_t spin();
	void release();

	std::vector<std::thread> m_threads;

	gdul::concurrent_queue<std::function<void()>> m_taskQueue;

	std::atomic<bool> m_inCommission;

	std::atomic<std::uint32_t> m_taskCounter;
};

thread_pool::thread_pool(std::uint32_t threads, std::uint32_t affinityBegin) :
	m_inCommission(true),
	m_taskCounter(0)
{
	const std::size_t numThreads(std::thread::hardware_concurrency());
	for (std::uint32_t i = 0; i < threads; ++i)
	{
		const std::uint64_t affinity((((std::uint64_t)affinityBegin + i) % numThreads));
		const std::uint64_t affinityMask((std::size_t(1) << affinity));
		m_threads.push_back(std::thread(&thread_pool::idle, this, affinityMask));
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

bool thread_pool::has_unfinished_tasks() const
{
	return 0 < m_taskCounter.load(std::memory_order_acquire);
}
void thread_pool::idle(std::uint64_t affinityMask)
{
	uint64_t result(0);

	do
	{
		result = SetThreadAffinityMask(GetCurrentThread(), affinityMask);
	} while (!result);


	while (m_inCommission.load(std::memory_order_relaxed) | (0 < m_taskCounter.load(std::memory_order_acquire)))
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
}
}