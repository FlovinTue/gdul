#pragma once

#include <functional>
#include <thread>
#include <concurrent_queue.h>
#include <vector>
#include <atomic>
#include <condition_variable>

class ThreadPool
{
public:
	ThreadPool(uint32_t aThreads = std::thread::hardware_concurrency(), float aSleepThreshHold = .25f);
	~ThreadPool();

	void AddTask(const std::function<void()>& aWorkFunction);
	void Decommission();

	bool HasUnfinishedTasks() const;
	uint32_t GetUnfinishedTasks() const;

private:
	void Idle();

	std::vector<std::thread> myThreads;

	concurrency::concurrent_queue<std::function<void()>> myTaskQueue;

	std::atomic<bool> myIsInCommission;

	const float mySleepThreshhold;

	std::condition_variable myWaitCondition;
	std::atomic<uint32_t> myTaskCounter;
};

