#pragma once

#include <functional>
#include <thread>
#include <atomic>
#include <concurrent_queue.h>
#include <vector>

class ThreadPool
{
public:
	ThreadPool(std::uint32_t aThreads = std::thread::hardware_concurrency(), std::uint32_t affinityBegin = 0);
	~ThreadPool();

	void AddTask(std::function<void()> aWorkFunction);
	void Decommission();

	bool HasUnfinishedTasks() const;
private:
	void Idle(std::uint64_t affinityMask);

	uint32_t spin();
	void release();

	std::vector<std::thread> myThreads;

	concurrency::concurrent_queue<std::function<void()>> myTaskQueue;

	std::atomic<bool> myIsInCommission;

	std::atomic<std::uint32_t> myTaskCounter;
};

