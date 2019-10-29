#include "pch.h"
#include "ThreadPool.h"
#include <Windows.h>

ThreadPool::ThreadPool(std::uint32_t aThreads, std::uint32_t affinityBegin) :
	myIsInCommission(true),
	myTaskCounter(0)
{
	const std::size_t numThreads(std::thread::hardware_concurrency());
	for (std::uint32_t i = 0; i < aThreads; ++i) {
		const std::uint64_t affinity(((affinityBegin + i) % numThreads));
		const std::uint64_t affinityMask((std::size_t(1) << affinity));
		myThreads.push_back(std::thread(&ThreadPool::Idle, this, affinityMask));
	}
}


ThreadPool::~ThreadPool()
{
	Decommission();
}

void ThreadPool::AddTask(std::function<void()> aWorkFunction)
{
	++myTaskCounter;
	myTaskQueue.push(aWorkFunction);
}

void ThreadPool::Decommission()
{
	myIsInCommission = false;

	for (size_t i = 0; i < myThreads.size(); ++i)
		myThreads[i].join();

	myThreads.clear();
}

bool ThreadPool::HasUnfinishedTasks() const
{
	return 0 < myTaskCounter.load(std::memory_order_acquire);
}
void ThreadPool::Idle(std::uint64_t affinityMask)
{
	uint64_t result(0);

	do {
		result = SetThreadAffinityMask(GetCurrentThread(), affinityMask);
	} while (!result);
	

	while (myIsInCommission.load(std::memory_order_relaxed) | (0 < myTaskCounter.load(std::memory_order_acquire)))
	{
		std::function<void()> task;
		if (myTaskQueue.try_pop(task))
		{
			task();
			--myTaskCounter;
		}
		else {
			std::this_thread::yield();
		}
	}
}
