#pragma once

#include <thread>
#include "ThreadPool.h"
#include <gdul\concurrent_queue\concurrent_queue.h>
#include "Timer.h"
#include <concurrent_queue.h>
#include <queue>
#include <mutex>


// Test queue

#ifdef MOODYCAMEL
#include <concurrentqueue.h>
#endif

template <class T>
class queue_mutex_wrapper
{
public:

	bool try_pop(T& out) {
		mtx.lock();
		if (myQueue.empty()) {
			mtx.unlock();
			return false;
		}
		out = myQueue.front();
		myQueue.pop();
		mtx.unlock();
		return true;
	}
	void push(T& in) { mtx.lock(); myQueue.push(in); mtx.unlock(); }
	void clear() {
		mtx.lock();
		while (!myQueue.empty())myQueue.pop();
		mtx.unlock();
	}

	std::mutex mtx;
	std::queue<T> myQueue;
};

const std::uint32_t Writes = 2048;
const std::uint32_t Writers = std::thread::hardware_concurrency() / 2;
const std::uint32_t Readers = std::thread::hardware_concurrency() / 2;
const std::uint32_t WritesPerThread(Writes / Writers);
const std::uint32_t ReadsPerThread(Writes / Readers);

template <class T, class Allocator>
class Tester
{
public:
	Tester(Allocator& alloc);
	~Tester();

	double ExecuteConcurrent(std::uint32_t runs);
	double ExecuteSingleThread(std::uint32_t runs);
	double ExecuteSingleProducerSingleConsumer(std::uint32_t runs);
	double ExecuteRead(std::uint32_t runs);
	double ExecuteWrite(std::uint32_t runs);

private:
	bool CheckResults() const;

	void Write(std::uint32_t writes);
	void Read(std::uint32_t writes);

#ifdef GDUL
	gdul::concurrent_queue<T, Allocator> myQueue;
#elif defined(MSC_RUNTIME)
	concurrency::concurrent_queue<T> myQueue;
#elif defined(MOODYCAMEL)
	moodycamel::ConcurrentQueue<T> myQueue;
#elif defined(MTX_WRAPPER)
	queue_mutex_wrapper<T> myQueue;
#endif

	ThreadPool myWriter;
	ThreadPool myReader;

	std::atomic<bool> myIsRunning;
	std::atomic<std::uint32_t> myWrittenSum;
	std::atomic<std::uint32_t> myReadSum;
	std::atomic<std::uint32_t> myThrown;
	std::atomic<std::uint32_t> myWaiting;
};

template<class T, class Allocator>
inline Tester<T, Allocator>::Tester(Allocator&
#ifdef GDUL	
	alloc
#endif
) :
	myIsRunning(false),
	myWriter(Writers, 0),
	myReader(Readers, Writers),
	myWrittenSum(0),
	myReadSum(0),
	myThrown(0),
	myWaiting(0)
#ifdef GDUL
	, myQueue(alloc)
#endif
{
	srand(static_cast<std::uint32_t>(time(0)));
}

template<class T, class Allocator>
inline Tester<T, Allocator>::~Tester()
{
	myWriter.Decommission();
	myReader.Decommission();
}
template<class T, class Allocator>
inline double Tester<T, Allocator>::ExecuteConcurrent(std::uint32_t runs)
{
#ifdef GDUL
	myQueue.unsafe_reset();
#endif

	double result(0.0);

	for (std::uint32_t i = 0; i < runs; ++i) {

		for (std::uint32_t j = 0; j < Writers; ++j)
			myWriter.AddTask(std::bind(&Tester::Write, this, WritesPerThread));
		for (std::uint32_t j = 0; j < Readers; ++j)
			myReader.AddTask(std::bind(&Tester::Read, this, ReadsPerThread));

		myWrittenSum = 0;
		myReadSum = 0;
		myThrown = 0;

		Timer timer;
		myIsRunning = true;

		while (myWriter.HasUnfinishedTasks() | myReader.HasUnfinishedTasks())
			std::this_thread::yield();

#ifdef GDUL
		myQueue.unsafe_clear();
#elif defined(MSC_RUNTIME) || defined(MTX_WRAPPER)
		myQueue.clear();
#endif
		myWaiting = 0;

		result += timer.GetTotalTime();

		myIsRunning = false;
	}

	std::cout << "Threw " << myThrown;
	if (!CheckResults()) {
		std::cout << " and failed check";
	}
	std::cout << std::endl;

	return result;
}
template<class T, class Allocator>
inline double Tester<T, Allocator>::ExecuteSingleThread(std::uint32_t runs)
{
#ifdef GDUL
	myQueue.unsafe_reset();
#endif
	double result(0.0);

	for (std::uint32_t i = 0; i < runs; ++i) {
		myWrittenSum = 0;
		myReadSum = 0;
		myThrown = 0;
		myWaiting = Readers + Writers;

		myIsRunning = true;

		Timer timer;

		Write(Writes);
		Read(Writes);

		result += timer.GetTotalTime();

		myWaiting = 0;

		myIsRunning = false;
	}

	return result;
}
template<class T, class Allocator>
inline double Tester<T, Allocator>::ExecuteSingleProducerSingleConsumer(std::uint32_t runs)
{
#ifdef GDUL
	myQueue.unsafe_reset();
#endif

	double result(0.0);

	for (std::uint32_t i = 0; i < runs; ++i) {

		myWrittenSum = 0;
		myReadSum = 0;
		myThrown = 0;
		myWaiting = Readers + Writers;


		myWriter.AddTask(std::bind(&Tester::Write, this, Writes));
		myReader.AddTask(std::bind(&Tester::Read, this, Writes));

		Timer timer;

		myIsRunning = true;

		while (myWriter.HasUnfinishedTasks() | myReader.HasUnfinishedTasks())
			std::this_thread::yield();

		result += timer.GetTotalTime();

		myWaiting = 0;

		myIsRunning = false;
	}

	return result;
}
template<class T, class Allocator>
inline double Tester<T, Allocator>::ExecuteRead(std::uint32_t runs)
{
#ifdef GDUL
	myQueue.unsafe_reset();
#endif

	double result(0.0);

	for (std::uint32_t i = 0; i < runs; ++i) {

		myWaiting = Readers + Writers;

		for (std::uint32_t j = 0; j < Writes; ++j) {
			T in;
			in.count = j;
#ifndef MOODYCAMEL
			myQueue.push(in);
#else
			myQueue.enqueue(in);
#endif
		}
		for (std::uint32_t j = 0; j < Readers; ++j)
			myReader.AddTask(std::bind(&Tester::Read, this, ReadsPerThread));

		Timer timer;
		myIsRunning = true;

		while (myReader.HasUnfinishedTasks())
			std::this_thread::yield();

		result += timer.GetTotalTime();

		myWaiting = 0;

		myIsRunning = false;
	}

	return result;
}
template<class T, class Allocator>
inline double Tester<T, Allocator>::ExecuteWrite(std::uint32_t runs)
{
#ifdef GDUL
	myQueue.unsafe_reset();
#endif

	double result(0.0);

	for (std::uint32_t i = 0; i < runs; ++i) {

		myWaiting = Readers + Writers;

		for (std::uint32_t j = 0; j < Writers; ++j)
			myWriter.AddTask(std::bind(&Tester::Write, this, WritesPerThread));

		Timer timer;
		myIsRunning = true;

		while (myWriter.HasUnfinishedTasks())
			std::this_thread::yield();

#ifdef GDUL
		//myQueue.unsafe_clear();
		myQueue.unsafe_reset();
#elif defined(MSC_RUNTIME)
		myQueue.clear();
#elif defined(MOODYCAMEL)
		T out;
		while (myQueue.try_dequeue(out));
#endif

		result += timer.GetTotalTime();

		myWaiting = 0;

		myIsRunning = false;
	}

	return result;
}
template<class T, class Allocator>
inline bool Tester<T, Allocator>::CheckResults() const
{
	if (myWrittenSum != myReadSum)
		return false;

	return true;
}
template<class T, class Allocator>
inline void Tester<T, Allocator>::Write(std::uint32_t writes)
{
#ifdef GDUL
	myQueue.reserve(writes);
#endif

	while (!myIsRunning);

	uint32_t sum(0);

#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	for (std::uint32_t j = 0; j < writes; ) {
		T in(1);
		try {
			myQueue.push(std::move(in));
			++j;
			sum += in.count;
		}
		catch (...) {
			++myThrown;
		}
	}
#else
	for (std::uint32_t j = 0; j < writes; ++j) {
		T in;
		in.count = j;
		sum += in.count;
#ifndef MOODYCAMEL
		myQueue.push(in);
#else
		myQueue.enqueue(in);
#endif
	}
#endif

	myWrittenSum += sum;

	++myWaiting;

	while (myWaiting < (Writers + Readers)) {
		std::this_thread::yield();
	}
}

template<class T, class Allocator>
inline void Tester<T, Allocator>::Read(std::uint32_t reads)
{
	while (!myIsRunning);
	
	uint32_t sum(0);

	T out{ 0 };
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
	for (std::uint32_t j = 0; j < reads;) {
		while (true) {
			try {
				if (myQueue.try_pop(out)) {
					++j;
					sum += out.count;
					break;
				}
			}
			catch (...) {
				++myThrown;
			}
		}
	}

#else
	for (std::uint32_t j = 0; j < reads; ++j) {
		while (true) {
#ifndef MOODYCAMEL
			if (myQueue.try_pop(out)) {
#else
			if (myQueue.try_dequeue(out)) {
#endif
				sum += out.count;
				break;
			}
			else {
				std::this_thread::yield();
			}
			}
		}
#endif
	myReadSum += sum;

	++myWaiting;

	while (myWaiting < (Writers + Readers)) {
		std::this_thread::yield();
	}
	}

