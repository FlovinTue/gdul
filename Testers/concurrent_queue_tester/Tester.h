#pragma once

#include <thread>
#include "../Common/thread_pool.h"
#include <gdul\concurrent_queue\concurrent_queue.h>
#include "../Common/Timer.h"
#include <concurrent_queue.h>
#include <queue>
#include <mutex>
#include <random>

// Test queue

#ifdef MOODYCAMEL
#include <concurrentqueue.h>
#endif

namespace gdul
{
extern std::random_device rd;
extern std::mt19937 rng;
}

template <class T>
class queue_mutex_wrapper
{
public:

	bool try_pop(T& out) {
		mtx.lock();
		if (m_queue.empty()) {
			mtx.unlock();
			return false;
		}
		out = m_queue.front();
		m_queue.pop();
		mtx.unlock();
		return true;
	}
	void push(T& in) { mtx.lock(); m_queue.push(in); mtx.unlock(); }
	void clear() {
		mtx.lock();
		while (!m_queue.empty())m_queue.pop();
		mtx.unlock();
	}

	std::mutex mtx;
	std::queue<T> m_queue;
};

const std::uint32_t Writes = 8192;
const std::uint32_t Writers = std::thread::hardware_concurrency() / 2;
const std::uint32_t Readers = std::thread::hardware_concurrency() / 2;
const std::uint32_t WritesPerThread(Writes / Writers);
const std::uint32_t ReadsPerThread(Writes / Readers);

template <class T, class Allocator>
class tester
{
public:
	tester(Allocator& alloc);
	~tester();

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
	gdul::concurrent_queue<T, Allocator> m_queue;
#elif defined(MSC_RUNTIME)
	concurrency::concurrent_queue<T> m_queue;
#elif defined(MOODYCAMEL)
	moodycamel::ConcurrentQueue<T> m_queue;
#elif defined(MTX_WRAPPER)
	queue_mutex_wrapper<T> m_queue;
#endif

	gdul::thread_pool m_writer;
	gdul::thread_pool m_reader;

	std::atomic<bool> m_isRunning;
	std::atomic<std::uint32_t> m_writtenSum;
	std::atomic<std::uint32_t> m_readSum;
	std::atomic<std::uint32_t> m_thrown;
	std::atomic<std::uint32_t> m_waiting;
};

template<class T, class Allocator>
inline tester<T, Allocator>::tester(Allocator&
#ifdef GDUL	
	alloc
#endif
) :
	m_isRunning(false),
	m_writer(Writers, 0),
	m_reader(Readers, Writers),
	m_writtenSum(0),
	m_readSum(0),
	m_thrown(0),
	m_waiting(0)
#ifdef GDUL
	, m_queue(alloc)
#endif
{
	srand(static_cast<std::uint32_t>(time(0)));
}

template<class T, class Allocator>
inline tester<T, Allocator>::~tester()
{
	m_writer.decommission();
	m_reader.decommission();
}
template<class T, class Allocator>
inline double tester<T, Allocator>::ExecuteConcurrent(std::uint32_t runs)
{
#ifdef GDUL
	m_queue.unsafe_reset();
#endif

	double result(0.0);
	m_thrown = 0;
	m_writtenSum = 0;
	m_readSum = 0;

	for (std::uint32_t i = 0; i < runs; ++i) {


		for (std::uint32_t j = 0; j < Writers; ++j)
			m_writer.add_task(std::bind(&tester::Write, this, WritesPerThread));
		for (std::uint32_t j = 0; j < Readers; ++j)
			m_reader.add_task(std::bind(&tester::Read, this, ReadsPerThread));

		gdul::timer<float> time;
		m_isRunning = true;

		while (m_writer.has_unfinished_tasks() | m_reader.has_unfinished_tasks())
			std::this_thread::yield();

#ifdef GDUL
		m_queue.unsafe_clear();
#elif defined(MSC_RUNTIME) || defined(MTX_WRAPPER)
		m_queue.clear();
#endif
		m_waiting = 0;

		result += time.get();

		m_isRunning = false;
	}

	std::cout << "ExecuteConcurrent Threw " << m_thrown;
	if (!CheckResults()) {
		std::cout << " and failed check";
	}
	std::cout << std::endl;

	return result;
}
template<class T, class Allocator>
inline double tester<T, Allocator>::ExecuteSingleThread(std::uint32_t runs)
{
#ifdef GDUL
	m_queue.unsafe_reset();
#endif
	double result(0.0);
	m_thrown = 0;
	m_writtenSum = 0;
	m_readSum = 0;

	for (std::uint32_t i = 0; i < runs; ++i) {
		m_waiting = Readers + Writers;

		m_isRunning = true;

		gdul::timer<float> time;

		Write(Writes);
		Read(Writes);

		result += time.get();

		m_waiting = 0;

		m_isRunning = false;
	}

	std::cout << "ExecuteSingleThread Threw " << m_thrown;
	if (!CheckResults()) {
		std::cout << " and failed check";
	}

	std::cout << std::endl;
	return result;
}
template<class T, class Allocator>
inline double tester<T, Allocator>::ExecuteSingleProducerSingleConsumer(std::uint32_t runs)
{
#ifdef GDUL
	m_queue.unsafe_reset();
#endif

	double result(0.0);
	m_thrown = 0;
	m_writtenSum = 0;
	m_readSum = 0;

	for (std::uint32_t i = 0; i < runs; ++i) {

		m_waiting = Readers + Writers - 2;

		m_writer.add_task(std::bind(&tester::Write, this, Writes));
		m_reader.add_task(std::bind(&tester::Read, this, Writes));

		gdul::timer<float> time;

		m_isRunning = true;

		while (m_writer.has_unfinished_tasks() | m_reader.has_unfinished_tasks())
			std::this_thread::yield();

		result += time.get();

		m_waiting = 0;

		m_isRunning = false;
	}

	std::cout << "ExecuteSingleProducerSingleConsumer Threw " << m_thrown;
	if (!CheckResults()) {
		std::cout << " and failed check";
	}
	std::cout << std::endl;

	return result;
}
template<class T, class Allocator>
inline double tester<T, Allocator>::ExecuteRead(std::uint32_t runs)
{
#ifdef GDUL
	m_queue.unsafe_reset();
#endif

	double result(0.0);
	m_thrown = 0;
	m_writtenSum = 0;
	m_readSum = 0;

	for (std::uint32_t i = 0; i < runs; ++i) {

		m_waiting = Readers + Writers;

		m_isRunning = true;

		Write(ReadsPerThread * Readers);

		m_isRunning = false;

		m_waiting = Writers;

		for (std::uint32_t j = 0; j < Readers; ++j)
			m_reader.add_task(std::bind(&tester::Read, this, ReadsPerThread));

		gdul::timer<float> time;
		m_isRunning = true;

		while (m_reader.has_unfinished_tasks())
			std::this_thread::yield();

		result += time.get();

		m_waiting = 0;

		m_isRunning = false;
	}

	std::cout << "ExecuteRead Threw " << m_thrown;
	if (!CheckResults()) {
		std::cout << " and failed check";
	}

	std::cout << std::endl;
	return result;
}
template<class T, class Allocator>
inline double tester<T, Allocator>::ExecuteWrite(std::uint32_t runs)
{
#ifdef GDUL
	m_queue.unsafe_reset();
#endif

	double result(0.0);
	m_thrown = 0;
	m_writtenSum = 0;
	m_readSum = 0;

	for (std::uint32_t i = 0; i < runs; ++i) {

		m_waiting = Writers;

		for (std::uint32_t j = 0; j < Writers; ++j)
			m_writer.add_task(std::bind(&tester::Write, this, WritesPerThread));

		gdul::timer<float> time;
		m_isRunning = true;

		while (m_writer.has_unfinished_tasks())
			std::this_thread::yield();

		result += time.get();

#ifdef GDUL
		m_queue.unsafe_clear();
#elif defined(MSC_RUNTIME)
		m_queue.clear();
#elif defined(MOODYCAMEL)
		T out;
		while (m_queue.try_dequeue(out));
#endif

		m_waiting = 0;

		m_isRunning = false;
	}

	std::cout << "ExecuteWrite Threw " << m_thrown << std::endl;

	return result;
}
template<class T, class Allocator>
inline bool tester<T, Allocator>::CheckResults() const
{
	if (m_writtenSum != m_readSum)
		return false;

	return true;
}
template<class T, class Allocator>
inline void tester<T, Allocator>::Write(std::uint32_t writes)
{
#ifdef GDUL
	m_queue.reserve(writes);
#endif

	++m_waiting;

	while (m_waiting < (Writers + Readers))
	{
		std::this_thread::yield();
	}

	while (!m_isRunning);

	uint32_t sum(0);

	uint32_t seed(gdul::rng());

#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	for (std::uint32_t j = 0; j < writes; ) {
		T in(seed % (j + 1));
		try {
			m_queue.push(in);
			++j;
			sum += in.count;
		}
		catch (...) {
			++m_thrown;
		}
	}
#else
	for (std::uint32_t j = 0; j < writes; ++j) {
		T in;
		in.count = seed % (j + 1);
		sum += in.count;
#ifndef MOODYCAMEL
		m_queue.push(in);
#else
		m_queue.enqueue(in);
#endif
	}
#endif

	m_writtenSum += sum;
}

template<class T, class Allocator>
inline void tester<T, Allocator>::Read(std::uint32_t reads)
{
	++m_waiting;

	while (m_waiting < (Writers + Readers))
	{
		std::this_thread::yield();
	}

	while (!m_isRunning);
	
	uint32_t sum(0);

	T out{ 0 };
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	for (std::uint32_t j = 0; j < reads;) {
		while (true) {
			try {
				if (m_queue.try_pop(out)) {
					++j;
					sum += out.count;
					break;
				}
			}
			catch (...) {
				++m_thrown;
			}
		}
	}

#else
	for (std::uint32_t j = 0; j < reads; ++j) {
		while (true) {
#ifndef MOODYCAMEL
			if (m_queue.try_pop(out)) {
#else
			if (m_queue.try_dequeue(out)) {
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
	m_readSum += sum;
	}

