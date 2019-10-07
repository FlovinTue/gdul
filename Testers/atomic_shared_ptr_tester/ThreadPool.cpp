#include "pch.h"
#include <mutex>
#include "ThreadPool.h"
#include "Timer.h"

ThreadPool::ThreadPool(uint32_t aThreads, float aSleepThreshhold) :
	myIsInCommission(true),
	myTaskCounter(0),
	mySleepThreshhold(aSleepThreshhold)
{
	for (uint32_t i = 0; i < aThreads; ++i)
		myThreads.push_back(std::thread(&ThreadPool::Idle, this));
}


ThreadPool::~ThreadPool()
{
	Decommission();
}

void ThreadPool::AddTask(const std::function<void()>& aWorkFunction)
{
	++myTaskCounter;
	myTaskQueue.push(aWorkFunction);
	myWaitCondition.notify_one();
}

void ThreadPool::Decommission()
{
	myIsInCommission = false;

	myWaitCondition.notify_all();

	for (uint16_t i = 0; i < myThreads.size(); ++i)
		myThreads[i].join();

	myThreads.clear();
}

bool ThreadPool::HasUnfinishedTasks() const
{
	return 0 < myTaskCounter._My_val;
}
uint32_t ThreadPool::GetUnfinishedTasks() const
{
	return myTaskCounter._My_val;
}
void ThreadPool::Idle()
{
	std::mutex mutex;
	std::unique_lock<std::mutex> lock(mutex);

	Timer sleepTimer;

	while (myIsInCommission._My_val | (0 < myTaskCounter._My_val))
	{
		std::function<void()> task;
		if (myTaskQueue.try_pop(task))
		{
			task();
			--myTaskCounter;
			sleepTimer.Reset();
		}
		else
		{
			if (mySleepThreshhold < sleepTimer.GetTotalTime())
			{
				myWaitCondition.wait_until(lock, std::chrono::high_resolution_clock::now() + std::chrono::duration<float, std::milli>(500));
			}
			else
				std::this_thread::yield();
		}

	}
}
